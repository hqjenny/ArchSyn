#define N 8
void gemsfdtd_int(
         int* Hx, int* Hy, int* Hz,\
         int* Ex, int* Ey, int* Ez, int Cbdy, int Cbdz, int Cbdx,\
         int* Ey1,  int* Ex1,  int* Ez1,  int* Ez2) {

    int nx = N;
    int ny = N;
    int nz = N;
    int k, j, i;
    for (i = 1; i<= nx; i++)
        for (j = 1; j<= ny; j++) {
            int Eycur = Ey[i*nz*ny+j*nz+1];
            int Excur = Ex[i*nz*ny+j*nz+1];
            for (k = 1; k <= nz; k++) {
                int Hxin =  Hx[i*nz*ny+j*nz+k];
                int Hyin =  Hy[i*nz*ny+j*nz+k];
                int Hzin =  Hz[i*nz*ny+j*nz+k];

                int Eyin = Eycur;
                int Eyin_kp1 = Ey[i*nz*ny+nz*j+k+1];
                Eycur = Eyin_kp1;
                int Eyin_ip1 = Ey1[(i+1)*nz*ny+j*nz+k];

                int Ezin = Ez[i*nz*ny+j*nz+k];
                int Ezinj_p1 = Ez1[i*nz*ny+(j+1)*nz+k];
                int Ezini_p1 = Ez2[(i+1)*nz*ny+j*nz+k];

                int Exin = Excur;
                int Exin_jp1 = Ex1[i*nz*ny+(j+1)*nz+k];
                int Exin_kp1 = Ex[i*nz*ny+j*nz+k+1];
                Excur = Exin_kp1;

                Hx[i*nz*ny+j*nz+k] = Hxin + ((Eyin_kp1-Eyin)*Cbdz + (Ezin-Ezinj_p1)*Cbdy);
                Hy[i*nz*ny+j*nz+k] = Hyin + ((Ezini_p1-Ezin)*Cbdx + (Exin-Exin_kp1)*Cbdz);
                Hz[i*nz*ny+j*nz+k] = Hzin + ((Exin_jp1-Exin)*Cbdy + (Eyin-Eyin_ip1)*Cbdx);
            }
        }
}
