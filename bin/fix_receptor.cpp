//Converts DOF so that receptor rotations and translations are zero

//usage: ./fix_receptor structures.dat <number of ligands>


#include "max.h"
#include <cmath>
#include <cstdio>

const double pi = 4.0f * atan(1.0f);

extern "C" void matinv_(double *rotmat);
extern "C" void matmult_(double *rotmat1, double *rotmat2, double *rotmat);
extern "C" void vecmatmult_(double *v0, double *m, double *v);

extern "C" FILE *read_dof_init_(const char *f_, int nlig, int &line, double (&pivot)[3][MAXLIG], int &auto_pivot, int &centered_receptor, int &centered_ligands, int f_len);

extern "C" int read_dof_(FILE *fil, int &line, int &nstruc, const char *f_, dof2 &phi, dof2 &ssi, dof2 &rot, dof2 &xa, dof2 &ya, dof2 &za, modes2 &dlig, const int &nlig, const int *nhm, int &seed, char *&label, int f_len);

extern "C" void euler2rotmat_(const double &phi,const double &ssi, const double &rot, double (&rotmat)[9]);

extern "C" void rotate_(const int &maxlig,const int &max3atom,double (&rotmat)[9],const double &xa,const double &ya,const double &za,
double *pivot,
int &ijk,int *ieins, double *x);

/* DOFs */
static double phi[MAXLIG];
static double ssi[MAXLIG];
static double rot[MAXLIG];
static double xa[MAXLIG];
static double ya[MAXLIG];
static double za[MAXLIG];
static double dlig[MAXLIG][MAXMODE];
static int seed;
static char *label;

#include <cstring>
#include <cstdlib>

void usage() {
  fprintf(stderr, "usage: $path/fix_receptor structures.dat <number of ligands>\n");
  exit(1);
}

bool exists(const char *f) {
  FILE *fil = fopen(f, "r");
  if (fil == NULL) return 0;
  else {
    fclose(fil);
    return 1;
  }
}

int main(int argc, char *argv[]) {
  int i;
  int nhm[MAXLIG];
  for (int n = 0; n < MAXLIG; n++) nhm[n] = 0;
  if (argc > 3 && (!strcmp(argv[3],"--modes"))) {
    for (int n = 0; n < argc-4; n++) {
      nhm[n] = atoi(argv[n+4]);
    }
    argc = 3;
  }
  if (argc != 3) {
    fprintf(stderr, "Wrong number of arguments\n"); usage();
  }
  if (!exists(argv[1])) {
    fprintf(stderr, "File %s does not exist\n", argv[1]);
    exit(1);
  }
  int nlig = atoi(argv[2]);

  //read DOFs and set pivots
  double pivot[3][MAXLIG];
  memset(pivot,0,sizeof(pivot));
  int auto_pivot, centered_receptor, centered_ligands;    
  int line;
  FILE *fil = read_dof_init_(argv[1], nlig, line, pivot, auto_pivot, centered_receptor, centered_ligands, strlen(argv[1]));
  if (centered_receptor != centered_ligands) { 
    fprintf(stderr, "Receptor and ligands must be both centered or both uncentered\n");
    exit(1);
  }  
  if (!(centered_ligands) && auto_pivot) {
    fprintf(stderr, "With uncentered ligands, pivots must be supplied\n");
    exit(1);
  }

  if (auto_pivot) printf("#pivot auto\n");
  else {
    for (i = 0; i < nlig; i++) {
      printf("#pivot %d %.3f %.3f %.3f\n", 
	i+1, pivot[0][i], pivot[1][i], pivot[2][i]);
    }   	
  }   
  if (centered_receptor) printf("#centered receptor: true\n");
  else printf("#centered receptor: false\n");
  if (centered_ligands) printf("#centered ligands: true\n");
  else printf("#centered ligands: false\n");
  //main loop  
  int nstruc = 0;
  while (1) {
    
    int result = read_dof_(fil, line, nstruc, argv[1], phi, ssi, rot, xa, ya, za, dlig, nlig, nhm, seed, label, strlen(argv[1]));
    if (result != 0) break;

    printf("#%d\n",nstruc);
    printf("### SEED %d\n", seed); 
    if (label != NULL) printf("%s",label);
    double rotmatr[9], rotmatrinv[9];
    euler2rotmat_(phi[0],ssi[0],rot[0],rotmatr); 
    memcpy(rotmatrinv,rotmatr,sizeof(rotmatr));
    matinv_(rotmatrinv);
        
    double xar = xa[0], yar = ya[0], zar = za[0];
    
    for (i = 0; i < nlig; i++) {
      
      //Compute rotation matrix
      double rotmatl[9];
      euler2rotmat_(phi[i],ssi[i],rot[i],rotmatl);      
     
      double rotmatd[9];
      matmult_(rotmatl, rotmatrinv,rotmatd);
      
      double dt[3] = {
        xa[i]-xar,
        ya[i]-yar,
	za[i]-zar
      };
      double dp[3] = {
        pivot[0][i]-pivot[0][0],
        pivot[1][i]-pivot[1][0],
	pivot[2][i]-pivot[2][0]
      };
      double td0[3] = {
        dt[0]+dp[0],
	dt[1]+dp[1],
	dt[2]+dp[2],
      };
      
      double td[3];
      vecmatmult_(td0,rotmatrinv,td);
      xa[i] = td[0]-dp[0];
      ya[i] = td[1]-dp[1];
      za[i] = td[2]-dp[2];
            
      phi[i] = atan2(rotmatd[5],rotmatd[2]);
      ssi[i] = acos(rotmatd[8]);
      rot[i] = atan2(-rotmatd[7],-rotmatd[6]);       
      
      if (fabs(rotmatd[8]) >= 0.999) { //gimbal lock
	phi[i] = 0;
	ssi[i] = 0;	
	if (rotmatd[0] >= 0.999) rot[i] = 0;
        else if (rotmatd[0] <= -0.999) rot[i] = pi;	
        else rot[i] = acos(rotmatd[0]);
      }
            
      printf("   %.8f %.8f %.8f %.4f %.4f %.4f", 
        phi[i], ssi[i], rot[i], 
        xa[i],  ya[i], za[i]      
      );
      for (int ii = 0; ii < nhm[i]; ii++) {
        printf(" %.4f", dlig[i][ii]);
      }
      printf("\n"); 

    }
  }
}