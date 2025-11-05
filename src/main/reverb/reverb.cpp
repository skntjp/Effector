#include <iostream>
#include <vector>
#include <cmath> // For std::pow
#include <numeric> // For std::accumulate
#include "portaudio.h"

#define FRAMES_PER_BUFFER   4096 // Adjust buffer size for better reverb processing
#define SAMPLE_RATE         44100
#define NUM_CHANNELS        1
#define PA_SAMPLE_TYPE      paFloat32

typedef float PA_SAMPLE;

// --- Reverb parameters (Adjust these values for different reverb effects) ---
const float REVERB_TIME = 1.0f; // 残響時間 (秒)
const float REVERB_LEVEL = 0.6f; // リバーブのレベル (0.0 - 1.0)

// --- Global variables for reverb (state) ---
// Primes up to 3571 (from Python code) - Used for delay lengths
const int prime_numbers[] = {
    2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71,
    73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151, 157, 163, 167, 173,
    179, 181, 191, 193, 197, 199, 211, 223, 227, 229, 233, 239, 241, 251, 257, 263, 269, 271, 277, 281,
    283, 293, 307, 311, 313, 317, 331, 337, 347, 349, 353, 359, 367, 373, 379, 383, 389, 397, 401, 409,
    419, 421, 431, 433, 439, 443, 449, 457, 461, 463, 467, 479, 487, 491, 499, 503, 509, 521, 523, 541,
    547, 557, 563, 569, 571, 577, 587, 593, 599, 601, 607, 613, 617, 619, 631, 641, 643, 647, 653, 659,
    661, 673, 677, 683, 691, 701, 709, 719, 727, 733, 739, 743, 751, 757, 761, 769, 773, 787, 797, 809,
    811, 821, 823, 827, 829, 839, 853, 857, 859, 863, 877, 881, 883, 887, 907, 911, 919, 929, 937, 941,
    947, 953, 967, 971, 977, 983, 991, 997, 1009, 1013, 1019, 1021, 1031, 1033, 1039, 1049, 1051, 1061, 1063, 1069,
    1087, 1091, 1093, 1097, 1103, 1109, 1117, 1123, 1129, 1151, 1153, 1163, 1171, 1181, 1187, 1193, 1201, 1213, 1217, 1223,
    1229, 1231, 1237, 1249, 1259, 1277, 1279, 1283, 1289, 1291, 1297, 1301, 1303, 1307, 1319, 1321, 1327, 1361, 1367, 1373,
    1381, 1399, 1409, 1423, 1427, 1429, 1433, 1439, 1447, 1451, 1453, 1459, 1471, 1481, 1483, 1487, 1489, 1493, 1499, 1511,
    1523, 1531, 1543, 1549, 1553, 1559, 1567, 1571, 1579, 1583, 1597, 1601, 1607, 1609, 1613, 1619, 1621, 1627, 1637, 1657,
    1663, 1667, 1669, 1693, 1697, 1699, 1709, 1721, 1723, 1733, 1741, 1747, 1753, 1759, 1777, 1783, 1787, 1789, 1801, 1811,
    1823, 1831, 1847, 1861, 1867, 1871, 1873, 1877, 1879, 1889, 1901, 1907, 1913, 1931, 1933, 1949, 1951, 1973, 1979, 1987,
    1993, 1997, 1999, 2003, 2011, 2017, 2027, 2029, 2039, 2053, 2063, 2069, 2081, 2083, 2087, 2089, 2099, 2111, 2113, 2129,
    2131, 2137, 2141, 2143, 2153, 2161, 2179, 2203, 2207, 2213, 2221, 2237, 2239, 2243, 2251, 2267, 2269, 2273, 2281, 2287,
    2293, 2297, 2309, 2311, 2333, 2339, 2341, 2347, 2351, 2357, 2371, 2377, 2381, 2383, 2389, 2393, 2399, 2411, 2417, 2423,
    2437, 2441, 2447, 2459, 2467, 2473, 2477, 2503, 2521, 2531, 2539, 2543, 2549, 2551, 2557, 2579, 2591, 2593, 2609, 2617,
    2621, 2633, 2647, 2657, 2659, 2663, 2671, 2677, 2683, 2687, 2689, 2693, 2699, 2707, 2711, 2713, 2719, 2729, 2731, 2741,
    2749, 2753, 2767, 2777, 2789, 2791, 2797, 2801, 2803, 2819, 2833, 2837, 2843, 2851, 2857, 2861, 2879, 2887, 2897, 2903,
    2909, 2917, 2927, 2939, 2953, 2957, 2963, 2969, 2971, 2999, 3001, 3011, 3019, 3023, 3037, 3041, 3049, 3061, 3067, 3079,
    3083, 3089, 3109, 3119, 3121, 3137, 3163, 3167, 3169, 3181, 3187, 3191, 3203, 3209, 3217, 3221, 3229, 3251, 3253, 3257,
    3259, 3271, 3299, 3301, 3307, 3313, 3319, 3323, 3329, 3331, 3343, 3347, 3359, 3361, 3371, 3373, 3389, 3391, 3407, 3413,
    3433, 3449, 3457, 3461, 3463, 3467, 3469, 3491, 3499, 3511, 3517, 3527, 3529, 3533, 3539, 3541, 3547, 3557, 3559, 3571
};
const int NUM_PRIMES = sizeof(prime_numbers) / sizeof(prime_numbers[0]);

// Delay line lengths for comb filters
std::vector<int> d(6);
// Gain values for comb and allpass filters
std::vector<float> g(6);

// Delay buffers for comb filters (u0, u1, u2, u3)
std::vector<std::vector<PA_SAMPLE>> comb_delay_buffers(4);
// Current write indices for comb filters
std::vector<int> comb_write_indices(4, 0);

// Delay buffers for allpass filters (v1, v2)
std::vector<std::vector<PA_SAMPLE>> allpass_delay_buffers(2);
// Current write indices for allpass filters
std::vector<int> allpass_write_indices(2, 0);

// Function to find the closest prime number
int round_to_prime_number(float x) {
    int val = static_cast<int>(round(x));
    int min_diff = abs(val - prime_numbers[0]);
    int closest_prime = prime_numbers[0];

    for (int i = 1; i < NUM_PRIMES; ++i) {
        int diff = abs(val - prime_numbers[i]);
        if (diff < min_diff) {
            min_diff = diff;
            closest_prime = prime_numbers[i];
        }
    }
    return closest_prime;
}

// Initialization function for reverb parameters and buffers
void initializeReverb() {
    std::vector<float> tau = { 0.030f, 0.035f, 0.040f, 0.045f, 0.005f, 0.0017f };

    d[0] = round_to_prime_number(static_cast<float>(SAMPLE_RATE) * tau[0]);
    d[1] = round_to_prime_number(static_cast<float>(SAMPLE_RATE) * tau[1]);
    d[2] = round_to_prime_number(static_cast<float>(SAMPLE_RATE) * tau[2]);
    d[3] = round_to_prime_number(static_cast<float>(SAMPLE_RATE) * tau[3]);
    d[4] = round_to_prime_number(static_cast<float>(SAMPLE_RATE) * tau[4]);
    d[5] = round_to_prime_number(static_cast<float>(SAMPLE_RATE) * tau[5]);

    g[0] = std::pow(10.0f, -3.0f * (static_cast<float>(d[0]) / SAMPLE_RATE) / REVERB_TIME);
    g[1] = std::pow(10.0f, -3.0f * (static_cast<float>(d[1]) / SAMPLE_RATE) / REVERB_TIME);
    g[2] = std::pow(10.0f, -3.0f * (static_cast<float>(d[2]) / SAMPLE_RATE) / REVERB_TIME);
    g[3] = std::pow(10.0f, -3.0f * (static_cast<float>(d[3]) / SAMPLE_RATE) / REVERB_TIME);
    g[4] = 0.7f;
    g[5] = 0.7f;

    // Allocate and clear delay buffers
    for (int i = 0; i < 4; ++i) {
        comb_delay_buffers[i].resize(d[i], 0.0f);
    }
    allpass_delay_buffers[0].resize(d[4], 0.0f); // v1 buffer
    allpass_delay_buffers[1].resize(d[5], 0.0f); // v2 buffer
}


static int patestCallback(const void* inputBuffer, void* outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData)
{
    const PA_SAMPLE* in = (const PA_SAMPLE*)inputBuffer;
    PA_SAMPLE* out = (PA_SAMPLE*)outputBuffer;

    PA_SAMPLE input_sample;
    PA_SAMPLE u_sum; // Sum of comb filter outputs (v0 in Python)
    PA_SAMPLE v1_out; // Output of the first allpass filter
    PA_SAMPLE v2_out; // Output of the second allpass filter (final reverb component)

    for (unsigned int i = 0; i < framesPerBuffer; i++) {
        input_sample = (inputBuffer == NULL) ? 0.0f : *in++;

        // --- Comb Filters (u0, u1, u2, u3) ---
        // Each comb filter processes the input_sample and its own delayed output
        u_sum = 0.0f;
        for (int k = 0; k < 4; ++k) {
            int read_index = (comb_write_indices[k] - d[k] + comb_delay_buffers[k].size()) % comb_delay_buffers[k].size();
            PA_SAMPLE delayed_sample = comb_delay_buffers[k][read_index];

            PA_SAMPLE current_output = input_sample + g[k] * delayed_sample;
            comb_delay_buffers[k][comb_write_indices[k]] = current_output; // Store current output in buffer
            u_sum += current_output; // Sum for v0
            comb_write_indices[k] = (comb_write_indices[k] + 1) % comb_delay_buffers[k].size();
        }

        // --- Allpass Filter 1 (v1) ---
        int read_index_v1 = (allpass_write_indices[0] - d[4] + allpass_delay_buffers[0].size()) % allpass_delay_buffers[0].size();
        PA_SAMPLE delayed_v1 = allpass_delay_buffers[0][read_index_v1];

        v1_out = -g[4] * u_sum + delayed_v1 + u_sum; // v1[n] = g[4] * v1[n - d[4]] - g[4] * v0[n] + v0[n - d[4]]

        allpass_delay_buffers[0][allpass_write_indices[0]] = u_sum; // Store v0[n]
        allpass_write_indices[0] = (allpass_write_indices[0] + 1) % allpass_delay_buffers[0].size();


        // --- Allpass Filter 2 (v2) ---
        int read_index_v2 = (allpass_write_indices[1] - d[5] + allpass_delay_buffers[1].size()) % allpass_delay_buffers[1].size();
        PA_SAMPLE delayed_v2 = allpass_delay_buffers[1][read_index_v2];

        v2_out = -g[5] * v1_out + delayed_v2 + v1_out; // v2[n] = g[5] * v2[n - d[5]] - g[5] * v1[n] + v1[n - d[5]]

        allpass_delay_buffers[1][allpass_write_indices[1]] = v1_out; // Store v1[n]
        allpass_write_indices[1] = (allpass_write_indices[1] + 1) % allpass_delay_buffers[1].size();

        // --- Final Output ---
        *out++ = input_sample + v2_out * REVERB_LEVEL;
    }

    return paContinue;
}

int main(void)
{
    PaStreamParameters inputParameters, outputParameters;
    PaStream* stream;
    PaError err;

    std::cout << "PortAudio テスト: マイク入力にリバーブを適用して出力します。" << std::endl;

    err = Pa_Initialize();
    if (err != paNoError) goto error;

    // Initialize reverb parameters and buffers BEFORE opening the stream
    initializeReverb();

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

    std::cout << "マイク入力の出力を開始しました。何か話してみてください。" << std::endl;
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