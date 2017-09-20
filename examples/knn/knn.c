#define DATA_SIZE 1200 
#define VEC_LEN 8

void knn_dist_ref(float* arg0, float* arg1, float* arg2){
    int i,j;
    // Calculate the distance
    for (i =0; i < DATA_SIZE; i ++)
    {
        float acc = 0;
        for (j = 0; j < VEC_LEN; j ++){
            float x = arg2[i*VEC_LEN+j] - arg1[j];
            acc += x * x;
        }
        arg0[i] = acc;
    }
}
