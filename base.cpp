#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <iostream>
#include <vector>
#include <queue>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <chrono>
#include <algorithm>

using namespace std;
namespace fs = std::filesystem;
using namespace std::chrono;

// ==========================
// ESTRUTURA
// ==========================

struct Box {
    int minx, miny, maxx, maxy;
};

// ==========================
// RGB → GRAY
// ==========================

void rgb2gray(unsigned char* input, unsigned char* gray, int w, int h) {
    for (int i = 0; i < w * h; i++) {
        int idx = i * 3;

        float r = input[idx];
        float g = input[idx + 1];
        float b = input[idx + 2];

        gray[i] = (unsigned char)(0.299f * r + 0.587f * g + 0.114f * b);
    }
}

// ==========================
// SOBEL
// ==========================

void sobel(unsigned char* gray, unsigned char* out, int w, int h) {

    int Gx[3][3] = {{-1,0,1},{-2,0,2},{-1,0,1}};
    int Gy[3][3] = {{-1,-2,-1},{0,0,0},{1,2,1}};

    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {

            int sumX = 0;
            int sumY = 0;

            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {

                    int pixel = gray[(y + ky) * w + (x + kx)];

                    sumX += pixel * Gx[ky + 1][kx + 1];
                    sumY += pixel * Gy[ky + 1][kx + 1];
                }
            }

            int mag = (int)sqrt(sumX * sumX + sumY * sumY);
            if (mag > 255) mag = 255;

            out[y * w + x] = (unsigned char)mag;
        }
    }
}

// ==========================
// THRESHOLD
// ==========================

void threshold_bin(unsigned char* in, unsigned char* bin, int w, int h, int T) {
    for (int i = 0; i < w * h; i++) {
        bin[i] = (in[i] > T) ? 255 : 0;
    }
}

// ==========================
// COMPONENTES CONECTADOS
// ==========================

vector<Box> findComponents(unsigned char* bin, int w, int h) {

    vector<Box> boxes;
    vector<int> visited(w * h, 0);

    int dx[4] = {1, -1, 0, 0};
    int dy[4] = {0, 0, 1, -1};

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {

            int idx = y * w + x;

            if (bin[idx] == 255 && visited[idx] == 0) {

                queue<pair<int,int>> q;
                q.push({x, y});
                visited[idx] = 1;

                Box b;
                b.minx = b.maxx = x;
                b.miny = b.maxy = y;

                int area = 0;

                while (!q.empty()) {

                    auto p = q.front();
                    q.pop();

                    int cx = p.first;
                    int cy = p.second;

                    area++;

                    b.minx = min(b.minx, cx);
                    b.miny = min(b.miny, cy);
                    b.maxx = max(b.maxx, cx);
                    b.maxy = max(b.maxy, cy);

                    for (int k = 0; k < 4; k++) {

                        int nx = cx + dx[k];
                        int ny = cy + dy[k];

                        if (nx >= 0 && ny >= 0 && nx < w && ny < h) {

                            int nidx = ny * w + nx;

                            if (bin[nidx] == 255 && visited[nidx] == 0) {
                                visited[nidx] = 1;
                                q.push({nx, ny});
                            }
                        }
                    }
                }

                if (area > 500) {
                    boxes.push_back(b);
                }
            }
        }
    }

    return boxes;
}

// ==========================
// DESENHAR CAIXAS
// ==========================

void draw_boxes(unsigned char* img, int w, int h, vector<Box>& boxes) {

    for (auto &b : boxes) {

        for (int x = b.minx; x <= b.maxx; x++) {

            int top = (b.miny * w + x) * 3;
            int bot = (b.maxy * w + x) * 3;

            img[top] = 255; img[top+1] = 0; img[top+2] = 0;
            img[bot] = 255; img[bot+1] = 0; img[bot+2] = 0;
        }

        for (int y = b.miny; y <= b.maxy; y++) {

            int left  = (y * w + b.minx) * 3;
            int right = (y * w + b.maxx) * 3;

            img[left]  = 255; img[left+1]  = 0; img[left+2]  = 0;
            img[right] = 255; img[right+1] = 0; img[right+2] = 0;
        }
    }
}

// ==========================
// MAIN
// ==========================

int main(int argc, char* argv[]) {

    int max_frames = -1;

    if (argc > 1) {
        max_frames = atoi(argv[1]);
        cout << "Modo teste: " << max_frames << " frames\n";
    }

    fs::create_directory("out");

    auto t0 = high_resolution_clock::now();

    int frame = 1;

    while (true) {

        if (max_frames != -1 && frame > max_frames) break;

        char filename[256];
        sprintf(filename, "frames/frame_%04d.png", frame);

        int w, h, c;

        unsigned char* input = stbi_load(filename, &w, &h, &c, 3);

        if (!input) {
            cout << "\nFim ou erro: " << filename << endl;
            break;
        }

        unsigned char* gray = new unsigned char[w*h];
        unsigned char* sob  = new unsigned char[w*h];
        unsigned char* bin  = new unsigned char[w*h];

        rgb2gray(input, gray, w, h);
        sobel(gray, sob, w, h);
        threshold_bin(sob, bin, w, h, 100);

        vector<Box> objects = findComponents(bin, w, h);

        draw_boxes(input, w, h, objects);

        char outname[256];
        sprintf(outname, "out/frame_%04d.png", frame);

        stbi_write_png(outname, w, h, 3, input, w * 3);

        cout << "\rProcessando: " << frame << flush;

        delete[] gray;
        delete[] sob;
        delete[] bin;
        stbi_image_free(input);

        frame++;
    }

    auto t1 = high_resolution_clock::now();
    double total_time = duration<double>(t1 - t0).count();

    cout << "\n\n===== FINAL =====\n";
    cout << "Frames processados: " << frame - 1 << endl;
    cout << "Tempo total: " << total_time << " s\n";

    return 0;
}
