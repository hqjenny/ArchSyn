#define N 64 
void conv( int* weight,  unsigned char * image1,  unsigned char* image2) {

    int x, y;
    int i, j;
    unsigned char pixel_value;
    // Generation of image2 after linear transformtion 
    for (y = 1; y < N - 1; y++) {
        for (x = 1; x < N - 1; x++) {
            pixel_value = 0.0;
            for (j = -1; j <= 1; j++) {
                for (i = -1; i <= 1; i++) {
                    pixel_value += weight[(j + 1)*3  + i + 1] * image1[(y + j) * N + x + i];
                }
            }
            image2[y * N +  x] = pixel_value;
        }
    }
}
