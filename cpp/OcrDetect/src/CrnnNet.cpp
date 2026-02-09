#include "CrnnNet.h"
#include "OcrUtils.h"
#include <fstream>
#include <numeric>

CrnnNet::CrnnNet() {}

CrnnNet::~CrnnNet() {
    delete session;
    free(inputName);
    free(outputName);
}

void CrnnNet::setNumThread(int numOfThread) {
    numThread = numOfThread;
    //===session options===
    // Sets the number of threads used to parallelize the execution within nodes
    // A value of 0 means ORT will pick a default
    //sessionOptions.SetIntraOpNumThreads(numThread);
    //set OMP_NUM_THREADS=16

    // Sets the number of threads used to parallelize the execution of the graph (across nodes)
    // If sequential execution is enabled this value is ignored
    // A value of 0 means ORT will pick a default
    sessionOptions.SetInterOpNumThreads(numThread);

    // Sets graph optimization level
    // ORT_DISABLE_ALL -> To disable all optimizations
    // ORT_ENABLE_BASIC -> To enable basic optimizations (Such as redundant node removals)
    // ORT_ENABLE_EXTENDED -> To enable extended optimizations (Includes level 1 + more complex optimizations like node fusions)
    // ORT_ENABLE_ALL -> To Enable All possible opitmizations
    sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
}

void CrnnNet::initModel(const std::string &pathStr, const std::string &keysPath) {
#ifdef _WIN32
    std::wstring crnnPath = strToWstr(pathStr);
    session = new Ort::Session(env, crnnPath.c_str(), sessionOptions);
#else
    session = new Ort::Session(env, pathStr.c_str(), sessionOptions);
#endif
    getInputName(session, inputName);
    getOutputName(session, outputName);

    //load keys
    std::ifstream in(keysPath.c_str());
    std::string line;
    if (in) {
        while (getline(in, line)) {// line中不包括每行的换行符
            // 去除可能的 \r (Windows 换行符兼容)
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            keys.push_back(line);
        }
    } else {
        fprintf(stderr, "keys file not found: %s\n", keysPath.c_str());
        return;
    }
    keys.push_back(" ");
    if (keys.empty()) {
        fprintf(stderr, "Error: keys file is empty or failed to load\n");
    }

}

template<class ForwardIterator>
inline static size_t argmax(ForwardIterator first, ForwardIterator last) {
    return std::distance(first, std::max_element(first, last));
}

TextLine CrnnNet::scoreToTextLine(const std::vector<float> &outputData, int h, int w) {
    int keySize = keys.size();
    std::string strRes;
    std::vector<float> scores;
    int lastIndex = 0;
    int maxIndex;
    float maxValue;

    for (int i = 0; i < h; i++) {
        maxIndex = 0;
        maxValue = -1000.f;
        
        // PPOCRv4 模型输出已经是 softmax 后的概率值
        // 直接找最大值及其索引，不需要再做 softmax
        // 这与 Python 版本的处理方式一致：preds_prob = preds.max(axis=2)
        for (int j = 0; j < w; j++) {
            float value = outputData[i * w + j];
            if (value > maxValue) {
                maxValue = value;
                maxIndex = j;
            }
        }
        
        // PPOCRv4: index 0 是 blank，字典字符从 index 1 开始
        // 字典有 keySize 个字符，对应 index 1 到 keySize
        // 修复边界条件: maxIndex <= keySize (而不是 < keySize)
        if (maxIndex > 0 && maxIndex <= keySize && (!(i > 0 && maxIndex == lastIndex))) {
            scores.emplace_back(maxValue);
            strRes.append(keys[maxIndex - 1]);
        }
        lastIndex = maxIndex;
    }
    return {strRes, scores};
}

TextLine CrnnNet::getTextLine(const cv::Mat &src) {
    // ===== 完全复现 Python OnnxOCR predict_rec.py 中的 resize_norm_img =====
    // Python: rec_image_shape = [3, 48, 320], rec_algorithm = 'SVTR_LCNet'
    int imgC = 3, imgH = dstHeight, imgW = 320;

    int h = src.rows, w = src.cols;
    float ratio = (float)w / (float)h;
    // Python: max_wh_ratio = max(imgW/imgH, w/h)
    float max_wh_ratio = (float)imgW / (float)imgH;
    if (ratio > max_wh_ratio) max_wh_ratio = ratio;
    // Python: imgW = int(imgH * max_wh_ratio)
    imgW = (int)(imgH * max_wh_ratio);

    // Python: resized_w = int(math.ceil(imgH * ratio))，但不超过 imgW
    int resized_w;
    if ((int)ceil(imgH * ratio) > imgW) {
        resized_w = imgW;
    } else {
        resized_w = (int)ceil(imgH * ratio);
    }

    cv::Mat srcResize;
    cv::resize(src, srcResize, cv::Size(resized_w, imgH));

    // Python 归一化:
    // resized_image = resized_image.astype('float32')
    // resized_image = resized_image.transpose((2, 0, 1)) / 255
    // resized_image -= 0.5
    // resized_image /= 0.5
    // 等价于: (pixel / 255.0 - 0.5) / 0.5 = pixel / 127.5 - 1.0
    // 与 substractMeanNormalize(meanValues={127.5}, normValues={1/127.5}) 完全一致

    // Python 零填充: padding_im = np.zeros((imgC, imgH, imgW)); padding_im[:,:,0:resized_w] = resized_image
    // CHW 格式，先对 resize 后的图像做归一化，然后右侧用 0.0 填充到 imgW
    size_t imageSize_resized = resized_w * imgH;
    size_t imageSize_padded = imgW * imgH;
    size_t numChannels = srcResize.channels();

    std::vector<float> inputTensorValues(numChannels * imageSize_padded, 0.0f);
    // 填充有效区域 (CHW 格式)
    for (int y = 0; y < imgH; y++) {
        for (int x = 0; x < resized_w; x++) {
            int pid = y * resized_w + x;
            for (size_t ch = 0; ch < numChannels; ch++) {
                float pixel = (float)srcResize.data[(y * resized_w + x) * numChannels + ch];
                float normalized = pixel / 127.5f - 1.0f;
                inputTensorValues[ch * imageSize_padded + y * imgW + x] = normalized;
            }
        }
    }
    // 右侧填充区域保持 0.0f（已在 vector 初始化时设为 0）

    std::array<int64_t, 4> inputShape{1, (int64_t)numChannels, imgH, imgW};

    auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);

    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(memoryInfo, inputTensorValues.data(),
                                                             inputTensorValues.size(), inputShape.data(),
                                                             inputShape.size());
    assert(inputTensor.IsTensor());

    auto outputTensor = session->Run(Ort::RunOptions{nullptr}, &inputName, &inputTensor, 1, &outputName, 1);

    assert(outputTensor.size() == 1 && outputTensor.front().IsTensor());

    std::vector<int64_t> outputShape = outputTensor[0].GetTensorTypeAndShapeInfo().GetShape();

    int64_t outputCount = std::accumulate(outputShape.begin(), outputShape.end(), 1,
                                          std::multiplies<int64_t>());

    float *floatArray = outputTensor.front().GetTensorMutableData<float>();
    std::vector<float> outputData(floatArray, floatArray + outputCount);

    int timeSteps, numClasses;
    if (outputShape.size() == 3) {
        if (outputShape[1] == 1) {
            timeSteps = outputShape[0];
            numClasses = outputShape[2];
        } else {
            timeSteps = outputShape[1];
            numClasses = outputShape[2];
        }
    } else if (outputShape.size() == 2) {
        timeSteps = outputShape[0];
        numClasses = outputShape[1];
    } else {
        timeSteps = outputShape[0];
        numClasses = outputShape[outputShape.size() - 1];
    }
    return scoreToTextLine(outputData, timeSteps, numClasses);
}

std::vector<TextLine> CrnnNet::getTextLines(std::vector<cv::Mat> &partImg, const char *path, const char *imgName) {
    int size = partImg.size();
    std::vector<TextLine> textLines(size);
    for (int i = 0; i < size; ++i) {
        //OutPut DebugImg
        if (isOutputDebugImg) {
            std::string debugImgFile = getDebugImgFilePath(path, imgName, i, "-debug-");
            saveImg(partImg[i], debugImgFile.c_str());
        }

        //getTextLine
        double startCrnnTime = getCurrentTime();
        TextLine textLine = getTextLine(partImg[i]);
        double endCrnnTime = getCurrentTime();
        textLine.time = endCrnnTime - startCrnnTime;
        textLines[i] = textLine;
    }
    return textLines;
}
