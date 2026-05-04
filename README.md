## Alguns comandos para facilitar a sua vida:

Para extrair frames do vídeo 'transito.mp4'

Use este comando:

```bash
./ffmpeg/ffmpeg -i transito.mp4 -r 30 frames/frame_%04d.png
```
-r 30 extrai 30 FPS 


Para transformar seus arquivos tratados em mp4 novamente use o comando:

```bash
./ffmpeg/ffmpeg -framerate 30 -i out/frame_%04d.png -c:v libx264 -crf 18 -pix_fmt yuv420p saida.mp4
```

# Cuidado para não subir as imgens e os videos no github, ele vai reclamar
