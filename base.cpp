
// STB para leitura de imagens (PNG, JPG etc.)
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// STB para escrita de imagens
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Bibliotecas padrão C++
#include <iostream>      // printf moderno (cout)
#include <vector>        // vetores dinâmicos
#include <queue>         // BFS (fila)
#include <cmath>         // sqrt
#include <cstdio>        // sprintf
#include <filesystem>    // criar pastas
#include <chrono>        // medir tempo
#include <algorithm>     // min/max

using namespace std;
namespace fs = std::filesystem;
using namespace std::chrono;


// Representa uma bounding box (retângulo do objeto detectado)
struct Box {
    int minx, miny; // canto superior esquerdo
    int maxx, maxy; // canto inferior direito
};


// Reduz imagem colorida (RGB) para escala de cinza
// Isso simplifica todo o processamento (1 canal ao invés de 3)
void rgb2gray(unsigned char* input, unsigned char* gray, int w, int h) {

    for (int i = 0; i < w * h; i++) {

        int idx = i * 3; // cada pixel tem 3 canais (R, G, B)

        float r = input[idx];
        float g = input[idx + 1];
        float b = input[idx + 2];

        // fórmula padrão de luminância (percepção humana)
        gray[i] = (unsigned char)(
            0.299f * r +
            0.587f * g +
            0.114f * b
        );
    }
}


// O Sobel detecta mudanças bruscas de intensidade → bordas
// usado para destacar contornos dos objetos 
void sobel(unsigned char* gray, unsigned char* out, int w, int h) {

    // máscaras de gradiente horizontal e vertical
    int Gx[3][3] = {{-1,0,1},{-2,0,2},{-1,0,1}};
    int Gy[3][3] = {{-1,-2,-1},{0,0,0},{1,2,1}};

    // percorre imagem ignorando bordas externas
    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {

            int sumX = 0; // gradiente horizontal
            int sumY = 0; // gradiente vertical

            // aplica convolução 3x3
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {

                    int pixel = gray[(y + ky) * w + (x + kx)];

                    sumX += pixel * Gx[ky + 1][kx + 1];
                    sumY += pixel * Gy[ky + 1][kx + 1];
                }
            }

            // magnitude do gradiente (força da borda)
            int mag = (int)sqrt(sumX * sumX + sumY * sumY);

            if (mag > 255) mag = 255;

            out[y * w + x] = (unsigned char)mag;
        }
    }
}


// Converte imagem em preto e branco:
// 255 = objeto / 0 = fundo
void threshold_bin(unsigned char* in, unsigned char* bin, int w, int h, int T) {

    for (int i = 0; i < w * h; i++) {

        bin[i] = (in[i] > T) ? 255 : 0;
    }
}


// Agrupa pixels conectados → forma objetos (blobs)
// Ex: cada carro vira um grupo
vector<Box> findComponents(unsigned char* bin, int w, int h) {

    vector<Box> boxes;              // lista de objetos detectados
    vector<int> visited(w * h, 0);  // marca pixels já visitados

    // vizinhança 4-direções (cima, baixo, esquerda, direita)
    int dx[4] = {1, -1, 0, 0};
    int dy[4] = {0, 0, 1, -1};

    // percorre toda a imagem
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {

            int idx = y * w + x;

            // encontrou pixel de objeto ainda não visitado
            if (bin[idx] == 255 && visited[idx] == 0) {

                queue<pair<int,int>> q;
                q.push({x, y});
                visited[idx] = 1;

                // bounding box inicial
                Box b;
                b.minx = b.maxx = x;
                b.miny = b.maxy = y;

                int area = 0;

                // BFS (expande região conectada)
                while (!q.empty()) {

                    auto p = q.front();
                    q.pop();

                    int cx = p.first;
                    int cy = p.second;

                    area++;

                    // atualiza limites do objeto
                    b.minx = min(b.minx, cx);
                    b.miny = min(b.miny, cy);
                    b.maxx = max(b.maxx, cx);
                    b.maxy = max(b.maxy, cy);

                    // explora vizinhos
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

                // remove ruído pequeno (evita falsos positivos)
                if (area > 500) {
                    boxes.push_back(b);
                }
            }
        }
    }

    return boxes;
}


// Desenha retângulos vermelhos nos objetos detectados
void draw_boxes(unsigned char* img, int w, int h, vector<Box>& boxes) {

    for (auto &b : boxes) {

        // linhas horizontais do retângulo
        for (int x = b.minx; x <= b.maxx; x++) {

            int top = (b.miny * w + x) * 3;
            int bot = (b.maxy * w + x) * 3;

            img[top] = 255; img[top+1] = 0; img[top+2] = 0;
            img[bot] = 255; img[bot+1] = 0; img[bot+2] = 0;
        }

        // linhas verticais do retângulo
        for (int y = b.miny; y <= b.maxy; y++) {

            int left  = (y * w + b.minx) * 3;
            int right = (y * w + b.maxx) * 3;

            img[left]  = 255; img[left+1]  = 0; img[left+2]  = 0;
            img[right] = 255; img[right+1] = 0; img[right+2] = 0;
        }
    }
}


int main(int argc, char* argv[]) {

    int max_frames = -1;

    // permite limitar frames 
    if (argc > 1) {
        max_frames = atoi(argv[1]);
        cout << "Modo teste: " << max_frames << " frames\n";
    }

    // cria pasta de saída
    fs::create_directory("out");

    // inicia medição de tempo total
    auto t0 = high_resolution_clock::now();

    int frame = 1;

    while (true) {

        if (max_frames != -1 && frame > max_frames)
            break;

        // monta nome do frame
        char filename[256];
        sprintf(filename, "frames/frame_%04d.png", frame);

        int w, h, c;

        // carrega imagem
        unsigned char* input = stbi_load(filename, &w, &h, &c, 3);

        // fim dos arquivos
        if (!input) {
            cout << "\nFim ou erro: " << filename << endl;
            break;
        }

        // buffers intermediários
        unsigned char* gray = new unsigned char[w*h];
        unsigned char* sob  = new unsigned char[w*h];
        unsigned char* bin  = new unsigned char[w*h];

        // pipeline de visão computacional
        rgb2gray(input, gray, w, h);        // 1. grayscale
        sobel(gray, sob, w, h);             // 2. bordas
        threshold_bin(sob, bin, w, h, 100);  // 3. binarização

        // 4. detecção de objetos
        vector<Box> objects = findComponents(bin, w, h);

        // 5. desenha resultado
        draw_boxes(input, w, h, objects);

        // salva frame processado
        char outname[256];
        sprintf(outname, "out/frame_%04d.png", frame);

        stbi_write_png(outname, w, h, 3, input, w * 3);

        cout << "\rProcessando: " << frame << flush;

        // libera memória
        delete[] gray;
        delete[] sob;
        delete[] bin;
        stbi_image_free(input);

        frame++;
    }

    // finaliza tempo total
    auto t1 = high_resolution_clock::now();
    double total_time = duration<double>(t1 - t0).count();

    cout << "\n\n===== FINAL =====\n";
    cout << "Frames processados: " << frame - 1 << endl;
    cout << "Tempo total: " << total_time << " s\n";

    return 0;
}
