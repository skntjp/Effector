#define _USE_MATH_DEFINES
#include <iostream>
#include <vector>
#include <cmath>
#include <numeric>

#include "portaudio.h"



#define FRAMES_PER_BUFFER   256
#define SAMPLE_RATE         44100
#define NUM_CHANNELS        2
#define PA_SAMPLE_TYPE      paFloat32

typedef float PA_SAMPLE;

// --- Distortion Parameters ---
const float DISTORTION_GAIN = 20.0f;    // 歪みの深さを調整 (tanhの入力に乗算)。大きくすると強く歪む。
const float OUTPUT_BOOST_GAIN = 2.0f;   // ★追加：歪んだ後の信号をブーストするゲイン。これで全体の音量を調整する。

// --- Oversampling Ratio ---
const int OVERSAMPLING_RATIO = 4;

// --- LPF Filter Coefficients (State variables for 2nd order IIR filter) ---
static float LPF1_a0[NUM_CHANNELS], LPF1_a1[NUM_CHANNELS], LPF1_a2[NUM_CHANNELS];
static float LPF1_b1[NUM_CHANNELS], LPF1_b2[NUM_CHANNELS];
static float LPF1_x1[NUM_CHANNELS], LPF1_x2[NUM_CHANNELS], LPF1_y1[NUM_CHANNELS], LPF1_y2[NUM_CHANNELS];

static float LPF2_a0[NUM_CHANNELS], LPF2_a1[NUM_CHANNELS], LPF2_a2[NUM_CHANNELS];
static float LPF2_b1[NUM_CHANNELS], LPF2_b2[NUM_CHANNELS];
static float LPF2_x1[NUM_CHANNELS], LPF2_x2[NUM_CHANNELS], LPF2_y1[NUM_CHANNELS], LPF2_y2[NUM_CHANNELS];

// Function to calculate 2nd order Butterworth Low-Pass Filter coefficients
void calculate_lpf_coeffs(float fs, float fc, float Q, float& a0, float& a1, float& a2, float& b1, float& b2) {
    float C = 1.0f / std::tan(M_PI * fc / fs);
    float C_squared = C * C;
    float D = 1.0f / (1.0f + 2.0f * C + C_squared); // Denominator

    a0 = 1.0f * D;
    a1 = 2.0f * D;
    a2 = 1.0f * D;
    b1 = 2.0f * (1.0f - C_squared) * D;
    b2 = (1.0f - 2.0f * C + C_squared) * D;
}

// Filter processing function for a single sample (IIR)
PA_SAMPLE process_filter(PA_SAMPLE input, float a0, float a1, float a2, float b1, float b2,
    float& x1, float& x2, float& y1, float& y2) {
    PA_SAMPLE output = a0 * input + a1 * x1 + a2 * x2 - b1 * y1 - b2 * y2;

    x2 = x1;
    x1 = input;
    y2 = y1;
    y1 = output;

    return output;
}

void initializeDistortion() {
    float fc1 = 2000.0f; // ディストーション前のLPFカットオフ
    float Q1 = 1.0f / std::sqrt(2.0f);

    float fc2 = 20000.0f / OVERSAMPLING_RATIO; // ディストーション後のLPFカットオフ
    float Q2 = 1.0f / std::sqrt(2.0f);

    calculate_lpf_coeffs(static_cast<float>(SAMPLE_RATE), fc1, Q1,
        LPF1_a0[0], LPF1_a1[0], LPF1_a2[0], LPF1_b1[0], LPF1_b2[0]);
    for (int c = 1; c < NUM_CHANNELS; ++c) {
        LPF1_a0[c] = LPF1_a0[0]; LPF1_a1[c] = LPF1_a1[0]; LPF1_a2[c] = LPF1_a2[0];
        LPF1_b1[c] = LPF1_b1[0]; LPF1_b2[c] = LPF1_b2[0];
    }

    calculate_lpf_coeffs(static_cast<float>(SAMPLE_RATE * OVERSAMPLING_RATIO), fc2, Q2,
        LPF2_a0[0], LPF2_a1[0], LPF2_a2[0], LPF2_b1[0], LPF2_b2[0]);
    for (int c = 1; c < NUM_CHANNELS; ++c) {
        LPF2_a0[c] = LPF2_a0[0]; LPF2_a1[c] = LPF2_a1[0]; LPF2_a2[c] = LPF2_a2[0];
        LPF2_b1[c] = LPF2_b1[0]; LPF2_b2[c] = LPF2_b2[0];
    }

    for (int c = 0; c < NUM_CHANNELS; ++c) {
        LPF1_x1[c] = LPF1_x2[c] = LPF1_y1[c] = LPF1_y2[c] = 0.0f;
        LPF2_x1[c] = LPF2_x2[c] = LPF2_y1[c] = LPF2_y2[c] = 0.0f;
    }
}

static int patestCallback(const void* inputBuffer, void* outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData)
{
    const PA_SAMPLE* in = (const PA_SAMPLE*)inputBuffer;
    PA_SAMPLE* out = (PA_SAMPLE*)outputBuffer;

    for (unsigned int i = 0; i < framesPerBuffer; i++) {
        for (unsigned int c = 0; c < NUM_CHANNELS; ++c) {
            PA_SAMPLE input_sample = (inputBuffer == NULL) ? 0.0f : in[i * NUM_CHANNELS + c];

            // --- Step 1: LPF (u0) ---
            PA_SAMPLE u0_sample = process_filter(input_sample,
                LPF1_a0[c], LPF1_a1[c], LPF1_a2[c],
                LPF1_b1[c], LPF1_b2[c],
                LPF1_x1[c], LPF1_x2[c], LPF1_y1[c], LPF1_y2[c]);

            PA_SAMPLE distorted_oversampled_output = 0.0f;

            // --- Step 2 & 3 & 4 & 5: Oversampling, Distortion, LPF2, Downsampling ---
            for (int k = 0; k < OVERSAMPLING_RATIO; ++k) {
                PA_SAMPLE u1_oversampled_input = (k == 0) ? u0_sample : 0.0f;

                PA_SAMPLE u2_oversampled = process_filter(u1_oversampled_input,
                    LPF2_a0[c], LPF2_a1[c], LPF2_a2[c],
                    LPF2_b1[c], LPF2_b2[c],
                    LPF2_x1[c], LPF2_x2[c], LPF2_y1[c], LPF2_y2[c]);

                // tanh関数の入力にDISTORTION_GAINを乗算
                PA_SAMPLE u3_distorted = std::tanh(5.0f * u2_oversampled * DISTORTION_GAIN / 2.0f);

                PA_SAMPLE u4_filtered = process_filter(u3_distorted,
                    LPF2_a0[c], LPF2_a1[c], LPF2_a2[c],
                    LPF2_b1[c], LPF2_b2[c],
                    LPF2_x1[c], LPF2_x2[c], LPF2_y1[c], LPF2_y2[c]);

                if (k == OVERSAMPLING_RATIO - 1) {
                    distorted_oversampled_output = u4_filtered;
                }
            }

            // --- Level Adjustment and Clipping Prevention ---
            // ★変更：OUTPUT_BOOST_GAINを適用
            PA_SAMPLE final_output_sample = distorted_oversampled_output * OUTPUT_BOOST_GAIN;

            // シンプルなクリッパーを適用 (デジタルクリッピング防止)
            if (final_output_sample > 1.0f) {
                final_output_sample = 1.0f;
            }
            else if (final_output_sample < -1.0f) {
                final_output_sample = -1.0f;
            }

            out[i * NUM_CHANNELS + c] = final_output_sample;
        }
    }

    return paContinue;
}

int main(void)
{
    PaStreamParameters inputParameters, outputParameters;
    PaStream* stream;
    PaError err;

    std::cout << "PortAudio テスト: マイク入力にディストーションを適用して出力します。" << std::endl;

    err = Pa_Initialize();
    if (err != paNoError) goto error;

    initializeDistortion();

    inputParameters.device = Pa_GetDefaultInputDevice();
    if (inputParameters.device == paNoDevice) {
        std::cerr << "エラー: デフォルトの入力デバイスが見つかりません。" << std::endl;
        goto error;
    }
    inputParameters.channelCount = NUM_CHANNELS;
    inputParameters.sampleFormat = PA_SAMPLE_TYPE;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    outputParameters.device = Pa_GetDefaultOutputDevice();
    if (outputParameters.device == paNoDevice) {
        std::cerr << "エラー: デフォルトの出力デバイスが見つかりません。" << std::endl;
        goto error;
    }
    outputParameters.channelCount = NUM_CHANNELS;
    outputParameters.sampleFormat = PA_SAMPLE_TYPE;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(
        &stream,
        &inputParameters,
        &outputParameters,
        SAMPLE_RATE,
        FRAMES_PER_BUFFER,
        paClipOff,
        patestCallback,
        NULL);
    if (err != paNoError) goto error;

    err = Pa_StartStream(stream);
    if (err != paNoError) goto error;

    std::cout << "マイク入力の出力を開始しました。何か話してみてください。" << std::endl; // Changed from std::cout << std::cout
    std::cout << "停止するには Enter キーを押してください。" << std::endl;
    getchar();

    err = Pa_StopStream(stream);
    if (err != paNoError) goto error;

    err = Pa_CloseStream(stream);
    if (err != paNoError) goto error;

    Pa_Terminate();
    std::cout << "プログラムを終了しました。" << std::endl;

    return 0;

error:
    Pa_Terminate();
    std::cerr << "PortAudio エラー: " << Pa_GetErrorText(err) << std::endl;
    return 1;
}