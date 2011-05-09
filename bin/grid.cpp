#include "grid.h"
#include "nonbon.h"
#include "state.h"
#include "prox.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <unistd.h>

void error(const char *filename) {
  fprintf(stderr, "Reading error in grid file %s\n", filename);
  exit(1);
}

void get_shm_name(int shm_id, char *shm_name) {
  sprintf(shm_name, "/attract-grid%d", shm_id);
}

void Grid::init_prox(int cartstatehandle,double proxlim0, double proxmax0, int proxmaxtype0) {
  prox = prox_init(cartstatehandle, plateaudissq, proxlim0, proxmax0, proxmaxtype0);
  proxlim = proxlim0;
  proxmax = proxmax0;
  if (proxmax < plateaudissq) {
    fprintf(stderr, "proxmax cannot be smaller than grid plateaudissq: %.3f < %.3f", proxmax, plateaudissq);
    exit(1);
  }
}

void Grid::init(double gridspacing0, int gridextension0, double plateaudis0,
double neighbourdis0, bool (&alphabet0)[MAXATOMTYPES]) {
  gridspacing = gridspacing0;
  gridextension = gridextension0;
  plateaudis = plateaudis0;
  plateaudissq = plateaudis*plateaudis;
  plateaudissqinv = 1.0/plateaudissq;
  neighbourdis = neighbourdis0;
  neighbourdissq = neighbourdis * neighbourdis;
  //Pre-compute the scale-down-distance ratios
  int size_ratio  = int(10000*plateaudissq);
  _ratio = new double[size_ratio];
  for (int n = 0; n <= size_ratio; n++) {
    double dissq = ((n+0.5)/10000);
    _ratio[n] = sqrt(dissq/plateaudissq) / (1/dissq) * (1/plateaudissq);
  }
  memcpy(alphabet, alphabet0, sizeof(alphabet));
  alphabetsize = 0;
  for (int n = 0; n < MAXATOMTYPES; n++) {
    if (alphabet[n]) {
      alphabetsize++;
    }
  }
}  

void Grid::read(const char *filename) {
  int n;  

  int read;
  
  FILE *f = fopen(filename, "rb");  
  if (f == NULL) error(filename);

  read = fread(&gridspacing, sizeof(double),1,f);  
  if (!read) error(filename);
  read = fread(&gridextension, sizeof(int),1,f);
  if (!read) error(filename);
  read = fread(&plateaudis, sizeof(double),1,f);  
  if (!read) error(filename);
  read = fread(&neighbourdis, sizeof(double),1,f);    
  if (!read) error(filename);
  bool alphabet0[MAXATOMTYPES];
  read = fread(&alphabet0, sizeof(alphabet0),1,f);    
  if (!read) error(filename);
  init(gridspacing, gridextension, plateaudis,neighbourdis,alphabet0);

  float arr1[3];
  read = fread(arr1, 3*sizeof(float),1,f);
  if (!read) error(filename);
  ori[0]=arr1[0];ori[1]=arr1[1];ori[2]=arr1[2];
  int arr2[6];
  read = fread(arr2, 6*sizeof(int),1,f);
  if (!read) error(filename);
  gridx=arr2[0];gridy=arr2[1];gridz=arr2[2];  
  gridx2=arr2[3];gridy2=arr2[4];gridz2=arr2[5];  
  read = fread(&natoms,sizeof(int),1,f);
  if (!read) error(filename);
  read = fread(&nhm,sizeof(int),1,f);
  if (!read) error(filename);
  if (nhm > 0) {
    modedofs = new double[nhm];
    read=fread(modedofs,nhm*sizeof(double),1,f);
    if (!read) error(filename);
  }
  read=fread(&pivot,sizeof(Coor),1,f);
  if (!read) error(filename);
  
  read = fread(&nr_energrads, sizeof(nr_energrads),1,f);  
  if (!read) error(filename);
  read = fread(&shm_energrads, sizeof(shm_energrads),1,f);  
  if (!read) error(filename);
  if (shm_energrads == -1) {
    energrads = new EnerGrad[nr_energrads];
    read = fread(energrads, nr_energrads*sizeof(EnerGrad),1,f);  
    if (!read) error(filename);
  }
  else {
    char shm_name[100];
    get_shm_name(shm_energrads, shm_name);  
    int fshm1 = shm_open(shm_name, O_RDONLY, S_IREAD);
    if (fshm1 == -1) {
      fprintf(stderr, "Reading error in grid file %s: shared memory segment %d for potential list does not exist\n", filename, shm_energrads);
      exit(1); 
    }   
    ftruncate(fshm1, nr_energrads*sizeof(EnerGrad));
    energrads = (EnerGrad *) mmap(0,nr_energrads*sizeof(EnerGrad),
     PROT_READ, MAP_SHARED | MAP_NORESERVE, fshm1, 0);
    if (energrads == NULL) {
      fprintf(stderr, "Reading error in grid file %s: Could not load shared memory segment %d\n", filename, shm_energrads);
      exit(1);       
    }  
  }  

  read = fread(&nr_neighbours, sizeof(nr_neighbours),1,f);  
  if (!read) error(filename);
  read = fread(&shm_neighbours, sizeof(shm_neighbours),1,f);  
  if (!read) error(filename);
  if (shm_neighbours == -1) {
    neighbours = new Neighbour[nr_neighbours];
    read = fread(neighbours, nr_neighbours*sizeof(Neighbour),1,f);
    if (!read) error(filename);
  }
  else {
    char shm_name[100];
    get_shm_name(shm_neighbours, shm_name);  
    int fshm2 = shm_open(shm_name, O_RDONLY, S_IREAD);
    if (fshm2 == -1) {
      fprintf(stderr, "Reading error in grid file %s: shared memory segment %d for neighbour list does not exist\n", filename, shm_neighbours);
      exit(1); 
    }   
    ftruncate(fshm2, nr_neighbours*sizeof(Neighbour));
    neighbours = (Neighbour *) mmap(0,nr_neighbours*sizeof(Neighbour),
     PROT_READ, MAP_SHARED | MAP_NORESERVE, fshm2, 0);
    if (neighbours == NULL) {
      fprintf(stderr, "Reading error in grid file %s: Could not load shared memory segment %d\n", filename, shm_neighbours);
      exit(1); 
    }  
  }  
  long innergridsize, biggridsize;
  read = fread(&innergridsize, sizeof(innergridsize),1,f);
  if (!read) error(filename);
  innergrid = new Voxel[innergridsize];
  read = fread(innergrid, innergridsize*sizeof(Voxel),1,f);
  if (!read) error(filename);
  read = fread(&biggridsize, sizeof(biggridsize),1,f);
  if (!read) error(filename);
  biggrid = new Potential[biggridsize];
  read = fread(biggrid, biggridsize*sizeof(Potential),1,f);
  if (!read) error(filename);
  fclose(f);

  Neighbour *nbnull = NULL;
  EnerGrad *egnull = NULL;
  
  for (n = 0; n < innergridsize; n++) {
    if (innergrid[n].potential[MAXATOMTYPES] != NULL) {
      for (int i = 0; i <= MAXATOMTYPES; i++) {
        if (i < MAXATOMTYPES && alphabet[i] == 0) continue;
        int dif = innergrid[n].potential[i]-egnull - 1;
	if (dif < 0 || dif  >= nr_energrads) {
	  fprintf(stderr, "Reading error in %s, innergrid voxel %d atom type %d: %d >= %d",             
	  filename, n+1, i+1, dif, nr_energrads);
	  exit(1);
	}	
        innergrid[n].potential[i] = energrads + dif;
      }
      if (innergrid[n].neighbourlist != NULL) {
        int dif = innergrid[n].neighbourlist-nbnull-1;
        innergrid[n].neighbourlist = neighbours + dif;
      }
    }  
  }
  for (n = 0; n < biggridsize; n++) {
    if (biggrid[n][MAXATOMTYPES] != NULL) {
      for (int i = 0; i <= MAXATOMTYPES; i++) {
        if (i < MAXATOMTYPES && alphabet[i] == 0) continue;
        int dif = biggrid[n][i]-egnull-1;
	if (dif < 0 || dif >= nr_energrads) {
	  fprintf(stderr, "Reading error in %s, biggrid voxel %d atom type %d: %d >= %d",             
	  filename, n+1, i+1, dif, nr_energrads);
	  exit(1);
	}
        biggrid[n][i] = energrads + dif;
      }
    }
  }
  init(gridspacing, gridextension, plateaudis,neighbourdis,alphabet0);
}

void Grid::write(const char *filename) {
  //NOTE: writing a grid makes it unusable!
  long n;
  long innergridsize = gridx*gridy*gridz;
  long biggridsize = gridx2*gridy2*gridz2;    
 
  FILE *f = fopen(filename, "wb");
  if (f == NULL) {
    fprintf(stderr, "Grid::write error for %s: Cannot open file for writing\n", filename);
    exit(1);
  }

  
  EnerGrad *shmptr1 = NULL; Neighbour *shmptr2 = NULL;
  if (shm_energrads != -1) {
    char shm_name[100];
    get_shm_name(shm_energrads, shm_name);
    int fshm1 = shm_open(shm_name, (O_CREAT | O_RDWR), (S_IREAD | S_IWRITE));
    if (fshm1 == -1) {
      fprintf(stderr, "Grid::write error for %s: Cannot open shared memory for writing\n", filename);
      exit(1);
    }     
    ftruncate(fshm1, nr_energrads*sizeof(EnerGrad));
    shmptr1 = (EnerGrad *) mmap(0,nr_energrads*sizeof(EnerGrad),(PROT_READ | PROT_WRITE),
     MAP_SHARED , fshm1, 0);
    if (shmptr1 == MAP_FAILED) {
      fprintf(stderr, "Grid::write error for %s: Cannot map shared memory for writing\n", filename);
      exit(1);
    } 
    memset(shmptr1,0,nr_energrads*sizeof(EnerGrad));
    close(fshm1);    
  }
  
  if (shm_neighbours != -1) {
    char shm_name[100];
    get_shm_name(shm_neighbours, shm_name);
    int fshm2 = shm_open(shm_name, (O_CREAT | O_RDWR), (S_IREAD | S_IWRITE));
    if (fshm2 == -1) {
      fprintf(stderr, "Grid::write error for %s: Cannot open shared memory for writing\n", filename);
      exit(1);
    }     
    ftruncate(fshm2, nr_neighbours*sizeof(Neighbour));
    shmptr2 = (Neighbour *) mmap(0,nr_neighbours*sizeof(Neighbour),(PROT_READ | PROT_WRITE),
     MAP_SHARED, fshm2, 0);
    if (shmptr2 == MAP_FAILED) {
      fprintf(stderr, "Grid::write error for %s: Cannot map shared memory for writing\n", filename);
      exit(1);
    } 
    memset(shmptr2,0,nr_neighbours*sizeof(Neighbour));
    close(fshm2);
  }  
    
  fwrite(&gridspacing, sizeof(double),1,f);  
  fwrite(&gridextension, sizeof(int),1,f);
  fwrite(&plateaudis, sizeof(double),1,f);  
  fwrite(&neighbourdis, sizeof(double),1,f);    
  fwrite(&alphabet, sizeof(alphabet),1,f);
  float arr1[] = {ori[0],ori[1],ori[2]};
  fwrite(arr1, 3*sizeof(float),1,f);
  int arr2[] = {gridx,gridy,gridz,gridx2,gridy2,gridz2};
  fwrite(arr2, 6*sizeof(int),1,f);
  fwrite(&natoms, sizeof(int),1,f); 
  fwrite(&nhm, sizeof(int),1,f);
  if (nhm > 0) fwrite(modedofs, sizeof(double) * nhm ,1,f);
  fwrite(&pivot,sizeof(Coor),1,f);
  
  nr_energrads = 0;  
  for (n = 0; n < innergridsize; n++) {
    if (innergrid[n].potential[MAXATOMTYPES] != NULL) nr_energrads+=alphabetsize+1;
  }
  for (n = 0; n < biggridsize; n++) {
    if (biggrid[n][MAXATOMTYPES] != NULL) nr_energrads+=alphabetsize+1;
  }  
  Neighbour *nbnull = NULL;
  EnerGrad *egnull = NULL;
  
  fwrite(&nr_energrads, sizeof(nr_energrads),1,f);
  fwrite(&shm_energrads, sizeof(shm_energrads),1,f);
    
    
  int curregcount = 1;
  for (n = 0; n < innergridsize; n++) {
    if (innergrid[n].potential[MAXATOMTYPES] != NULL) {
      for (int i = 0; i <= MAXATOMTYPES; i++) {
        if (i < MAXATOMTYPES && alphabet[i] == 0) continue;
	if (shm_energrads == -1)
          fwrite(innergrid[n].potential[i],sizeof(EnerGrad),1,f);
	else {
          memcpy(shmptr1, innergrid[n].potential[i],sizeof(EnerGrad));
	  shmptr1++;
	}
        innergrid[n].potential[i] = egnull + curregcount;
        curregcount++;	 
      }
      if (innergrid[n].neighbourlist != NULL) {
        if (innergrid[n].neighbourlist - neighbours > nr_neighbours) {
	  printf("ERR %d %d\n", innergrid[n].neighbourlist - &neighbours[0], nr_neighbours);
	}
        innergrid[n].neighbourlist = nbnull + (innergrid[n].neighbourlist - neighbours + 1);
      }
    }
  }

  for (n = 0; n < biggridsize; n++) {
    if (biggrid[n][MAXATOMTYPES] != NULL) {
      for (int i = 0; i <= MAXATOMTYPES; i++) {
        if (i < MAXATOMTYPES && alphabet[i] == 0) continue;
	if (shm_energrads == -1)
          fwrite(biggrid[n][i],sizeof(EnerGrad),1,f);
	else {
          memcpy(shmptr1, biggrid[n][i],sizeof(EnerGrad));
	  shmptr1++;
	}
        biggrid[n][i] = egnull + curregcount;
        curregcount++;	 
      }
    }
  }
  
  fwrite(&nr_neighbours, sizeof(nr_neighbours),1,f);
  fwrite(&shm_neighbours, sizeof(shm_neighbours),1,f);
  if (shm_neighbours == -1) 
     fwrite(neighbours, nr_neighbours*sizeof(Neighbour), 1, f);
  else 
    memcpy(shmptr2, neighbours, nr_neighbours*sizeof(Neighbour));
    
  fwrite(&innergridsize, sizeof(innergridsize),1,f);
  fwrite(innergrid, innergridsize*sizeof(Voxel),1,f);
  fwrite(&biggridsize, sizeof(biggridsize),1,f);
  fwrite(biggrid, biggridsize*sizeof(Potential),1,f);
  fclose(f);
}

#ifdef TORQUEGRID
inline void add_potential(Potential &p, int iab, double charge, int atomtype, double w, int fixre, double &evdw, double &eelec, Coor &grad, double *deltar, double (&rtorques)[3][3]) {
#else
inline void add_potential(Potential &p, int iab, double charge, int atomtype, double w, int fixre, double &evdw, double &eelec, Coor &grad, double *deltar) {
#endif
  if (p[atomtype] == NULL) {
    evdw  += w * 999999;
  }
  else {
    EnerGrad &e = *(p[atomtype]);
    evdw  += w * e.energy;
    if (iab) {
      Coor dgrad = {
        w * e.grad[0],
	w * e.grad[1],
	w * e.grad[2]
      };
      grad[0] += dgrad[0];
      grad[1] += dgrad[1];
      grad[2] += dgrad[2];
      if (!fixre) {
	deltar[3] -= dgrad[0];
	deltar[4] -= dgrad[1];
	deltar[5] -= dgrad[2];
#ifdef TORQUEGRID
	rtorques[0][0] += w * e.torques[0];
	rtorques[0][1] += w * e.torques[1];
	rtorques[0][2] += w * e.torques[2];
	rtorques[1][0] += w * e.torques[3];
	rtorques[1][1] += w * e.torques[4];
	rtorques[1][2] += w * e.torques[5];
	rtorques[2][0] += w * e.torques[6];
	rtorques[2][1] += w * e.torques[7];
	rtorques[2][2] += w * e.torques[8];
#endif      
      }
    }
    if (fabs(charge) > 0.001) {
      double wcharge = w * charge;
      eelec  += wcharge * p[MAXATOMTYPES]->energy;
      if (iab) {
	Coor dgrad = {
          wcharge * p[MAXATOMTYPES]->grad[0],
	  wcharge * p[MAXATOMTYPES]->grad[1],
	  wcharge * p[MAXATOMTYPES]->grad[2]
	};
	grad[0] += dgrad[0];
	grad[1] += dgrad[1];
	grad[2] += dgrad[2];
	if (!fixre) {
	  deltar[3] -= dgrad[0];
	  deltar[4] -= dgrad[1];
	  deltar[5] -= dgrad[2];
#ifdef TORQUEGRID	
	  rtorques[0][0] += wcharge * p[MAXATOMTYPES]->torques[0];
	  rtorques[0][1] += wcharge * p[MAXATOMTYPES]->torques[1];
	  rtorques[0][2] += wcharge * p[MAXATOMTYPES]->torques[2];
	  rtorques[1][0] += wcharge * p[MAXATOMTYPES]->torques[3];
	  rtorques[1][1] += wcharge * p[MAXATOMTYPES]->torques[4];
	  rtorques[1][2] += wcharge * p[MAXATOMTYPES]->torques[5];
	  rtorques[2][0] += wcharge * p[MAXATOMTYPES]->torques[6];
	  rtorques[2][1] += wcharge * p[MAXATOMTYPES]->torques[7];
	  rtorques[2][2] += wcharge * p[MAXATOMTYPES]->torques[8];      	
#endif	
        }
      }	
    }
  }
}

#ifdef TORQUEGRID
inline void add_potential2(Potential p, int iab, double charge, int atomtype, double w, int fixre, double &evdw, double &eelec, Coor &grad, double *deltar, double (&rtorques)[3][3]) {
#else
inline void add_potential2(Potential p, int iab, double charge, int atomtype, double w, int fixre, double &evdw, double &eelec, Coor &grad, double *deltar) {
#endif
  if (p[atomtype] != NULL) {
    EnerGrad &e = *(p[atomtype]);
    evdw  += w * e.energy;
    if (iab) {
      Coor dgrad = {
        w * e.grad[0],
	w * e.grad[1],
	w * e.grad[2]
      };
      grad[0] += dgrad[0];
      grad[1] += dgrad[1];
      grad[2] += dgrad[2];
      if (!fixre) {
	deltar[3] -= dgrad[0];
	deltar[4] -= dgrad[1];
	deltar[5] -= dgrad[2];
#ifdef TORQUEGRID      
	rtorques[0][0] += w * e.torques[0];
	rtorques[0][1] += w * e.torques[1];
	rtorques[0][2] += w * e.torques[2];
	rtorques[1][0] += w * e.torques[3];
	rtorques[1][1] += w * e.torques[4];
	rtorques[1][2] += w * e.torques[5];
	rtorques[2][0] += w * e.torques[6];
	rtorques[2][1] += w * e.torques[7];
	rtorques[2][2] += w * e.torques[8];      
#endif      
     }
    }
    if (fabs(charge) > 0.001) {
      double wcharge = w * charge;
      EnerGrad &e = *(p[MAXATOMTYPES]);
      eelec  += wcharge * e.energy;
      if (iab) {
	Coor dgrad = {
          wcharge * e.grad[0],
	  wcharge * e.grad[1],
	  wcharge * e.grad[2]
	};
	grad[0] += dgrad[0];
	grad[1] += dgrad[1];
	grad[2] += dgrad[2];
	if (!fixre) {	
	  deltar[3] -= dgrad[0];
	  deltar[4] -= dgrad[1];
	  deltar[5] -= dgrad[2];
#ifdef TORQUEGRID	
	  rtorques[0][0] += wcharge * e.torques[0];
	  rtorques[0][1] += wcharge * e.torques[1];
	  rtorques[0][2] += wcharge * e.torques[2];
	  rtorques[1][0] += wcharge * e.torques[3];
	  rtorques[1][1] += wcharge * e.torques[4];
	  rtorques[1][2] += wcharge * e.torques[5];
	  rtorques[2][0] += wcharge * e.torques[6];
	  rtorques[2][1] += wcharge * e.torques[7];
	  rtorques[2][2] += wcharge * e.torques[8];	
#endif	
        }
      }
    }    
  }
}

#ifdef TORQUEGRID
inline void trilin(
  const Coor &d, int atomtype, double charge0, double charge, double wel, //ligand
  const Grid &g, bool rigid, const Coor *xr, const double *wer, const double *chair, const int *iacir,  //receptor

  const Parameters &rc, const Parameters &ac, const Parameters &emin, const Parameters &rmin2,
  const iParameters &ipon, int potshape, //ATTRACT params

  int iab, int fixre, double &evdw, double &eelec, Coor &grad, //output

  Coor *fr, const double (&pm2)[3][3][3], double *deltar, double (&rtorques)[3][3] //receptor
) 
#define TORQUEARGS ,rtorques  
#else
inline void trilin(
  const Coor &d, int atomtype, double charge0, double charge, double wel, //ligand
  const Grid &g, bool rigid, const Coor *xr, const double *wer, const double *chair, const int *iacir,  //receptor

  const Parameters &rc, const Parameters &ac, const Parameters &emin, const Parameters &rmin2,
  const iParameters &ipon, int potshape, //ATTRACT params

  int iab, int fixre, double &evdw, double &eelec, Coor &grad, //output
  
  Coor *fr, double *deltar //receptor
) 
#define TORQUEARGS 
#endif
{
  Prox &prox = *g.prox;
  double ax = (d[0]-g.ori[0])/g.gridspacing;
  double ay = (d[1]-g.ori[1])/g.gridspacing;
  double az = (d[2]-g.ori[2])/g.gridspacing;

  bool chargenonzero = (fabs(charge0) > 0.001);

  double aax = (ax+g.gridextension)/2;  
  if (aax < 0 || aax >= g.gridx2-1) return;
  double aay = (ay+g.gridextension)/2;
  if (aay < 0 || aay >= g.gridy2-1) return;
  double aaz = (az+g.gridextension)/2;  
  if (aaz < 0 || aaz >= g.gridz2-1) return;
  
  bool inside = ( 
    (ax >= 0 && ax < g.gridx-1) &&
    (ay >= 0 && ay < g.gridy-1) &&
    (az >= 0 && az < g.gridz-1) 
  );

  int gx,gy,gz;
  if (!inside) {
    ax = aax; ay = aay; az = aaz;
    gx = g.gridx2; gy = g.gridy2; gz = g.gridz2;
  }
  else {
    gx = g.gridx; gy = g.gridy; gz = g.gridz;  
  }
  
  int px0 = floor(ax);  
  int px1 = ceil(ax);
  double wx1 = ax - px0;

  int py0 = floor(ay);  
  int py1 = ceil(ay);
  double wy1 = ay - py0;

  int pz0 = floor(az);  
  int pz1 = ceil(az);
  double wz1 = az - pz0;    
  double wx0 = 1-wx1, wy0 = 1-wy1, wz0 = 1-wz1;

  wx0 *= wel; wy0 *= wel; wz0 *= wel;
  wx1 *= wel; wy1 *= wel; wz1 *= wel;
  
  Potential *p;
  if (inside) {
    p = &g.innergrid[px0+gx*py0+gx*gy*pz0].potential;
    add_potential(*p,iab,charge,atomtype,wx0*wy0*wz0, fixre, evdw, eelec, grad ,deltar TORQUEARGS);
    p = &g.innergrid[px0+gx*py0+gx*gy*pz1].potential;
    add_potential(*p,iab,charge,atomtype,wx0*wy0*wz1, fixre, evdw, eelec, grad ,deltar TORQUEARGS);
    p = &g.innergrid[px0+gx*py1+gx*gy*pz0].potential;
    add_potential(*p,iab,charge,atomtype,wx0*wy1*wz0, fixre, evdw, eelec, grad ,deltar TORQUEARGS);
    p = &g.innergrid[px0+gx*py1+gx*gy*pz1].potential;
    add_potential(*p,iab,charge,atomtype,wx0*wy1*wz1, fixre, evdw, eelec, grad ,deltar TORQUEARGS);
    p = &g.innergrid[px1+gx*py0+gx*gy*pz0].potential;
    add_potential(*p,iab,charge,atomtype,wx1*wy0*wz0, fixre, evdw, eelec, grad ,deltar TORQUEARGS);
    p = &g.innergrid[px1+gx*py0+gx*gy*pz1].potential;
    add_potential(*p,iab,charge,atomtype,wx1*wy0*wz1, fixre, evdw, eelec, grad ,deltar TORQUEARGS);
    p = &g.innergrid[px1+gx*py1+gx*gy*pz0].potential;
    add_potential(*p,iab,charge,atomtype,wx1*wy1*wz0, fixre, evdw, eelec, grad ,deltar TORQUEARGS);
    p = &g.innergrid[px1+gx*py1+gx*gy*pz1].potential;
    add_potential(*p,iab,charge,atomtype,wx1*wy1*wz1, fixre, evdw, eelec, grad ,deltar TORQUEARGS);
  }
  else {
    p = &g.biggrid[px0+gx*py0+gx*gy*pz0];
    add_potential2(*p,iab,charge,atomtype,wx0*wy0*wz0, fixre, evdw, eelec, grad ,deltar TORQUEARGS);    
    p = &g.biggrid[px0+gx*py0+gx*gy*pz1];
    add_potential2(*p,iab,charge,atomtype,wx0*wy0*wz1, fixre, evdw, eelec, grad ,deltar TORQUEARGS);
    p = &g.biggrid[px0+gx*py1+gx*gy*pz0];
    add_potential2(*p,iab,charge,atomtype,wx0*wy1*wz0, fixre, evdw, eelec, grad ,deltar TORQUEARGS);
    p = &g.biggrid[px0+gx*py1+gx*gy*pz1];
    add_potential2(*p,iab,charge,atomtype,wx0*wy1*wz1, fixre, evdw, eelec, grad ,deltar TORQUEARGS);
    p = &g.biggrid[px1+gx*py0+gx*gy*pz0];
    add_potential2(*p,iab,charge,atomtype,wx1*wy0*wz0, fixre, evdw, eelec, grad ,deltar TORQUEARGS);
    p = &g.biggrid[px1+gx*py0+gx*gy*pz1];
    add_potential2(*p,iab,charge,atomtype,wx1*wy0*wz1, fixre, evdw, eelec, grad ,deltar TORQUEARGS);
    p = &g.biggrid[px1+gx*py1+gx*gy*pz0];
    add_potential2(*p,iab,charge,atomtype,wx1*wy1*wz0, fixre, evdw, eelec, grad ,deltar TORQUEARGS);
    p = &g.biggrid[px1+gx*py1+gx*gy*pz1];
    add_potential2(*p,iab,charge,atomtype,wx1*wy1*wz1, fixre, evdw, eelec, grad ,deltar TORQUEARGS);
  }

  if (inside) {
    int pxj = floor(ax+0.5);
    int pyj = floor(ay+0.5);
    int pzj = floor(az+0.5);
    if (pxj < g.gridx && pyj < g.gridy && pzj < g.gridz) {
      int index = pxj+gx*pyj+gx*gy*pzj;
      Voxel &v = g.innergrid[index];
      if (v.nr_neighbours > 0) {
        for (int i = 0; i < v.nr_neighbours; i++) {
	  Neighbour &nb = v.neighbourlist[i];
	  if (rigid && nb.type == 2) continue;

	  const Coor &dd = xr[nb.index];

	  Coor dis = {d[0] - dd[0], 
	              d[1] - dd[1], 
		      d[2] - dd[2], 
		      };
	  
	  double dsq = dis[0]*dis[0]+dis[1]*dis[1]+dis[2]*dis[2];
          if (dsq > g.plateaudissq) continue;
	  
	  int atomtype2 = iacir[nb.index]-1;
	  double ww = wel * wer[nb.index];

  	  Coor &gradr = fr[nb.index];
	  if (dsq <= g.proxlim) { //only for distance within 6A
            double rci = rc[atomtype][atomtype2];
            double aci = ac[atomtype][atomtype2];
            double emini = emin[atomtype][atomtype2];
            double rmin2i = rmin2[atomtype][atomtype2];
            int ivor = ipon[atomtype][atomtype2];
	  
	    double rr2 = 1.0/dsq;
	    dis[0] *= rr2; dis[1] *= rr2; dis[2] *= rr2;

	    double evdw0; Coor grad0;
            nonbon(iab,ww,rci,aci,emini,rmin2i,ivor, dsq, rr2, 
	      dis[0], dis[1], dis[2], potshape, evdw0, grad0);	
	    evdw += evdw0;
	    grad[0] += grad0[0];
	    grad[1] += grad0[1];
	    grad[2] += grad0[2];
            if (!fixre) {
	      gradr[0] -= grad0[0];
	      gradr[1] -= grad0[1];
	      gradr[2] -= grad0[2];
            }
	    double r = g.get_ratio(dsq);
	    Coor rdis = {r*dis[0], r*dis[1], r*dis[2]};  
	    nonbon(iab,ww,rci,aci,emini,rmin2i,ivor, g.plateaudissq, 
  	      g.plateaudissqinv,
	      rdis[0], rdis[1], rdis[2], potshape, evdw0, grad0);
	    evdw -= evdw0;
	    grad[0] -= grad0[0];
	    grad[1] -= grad0[1];
	    grad[2] -= grad0[2];
            if (!fixre) {
	      gradr[0] += grad0[0];
	      gradr[1] += grad0[1];
	      gradr[2] += grad0[2];
            }
	    if (chargenonzero) {
	      double c = charge0 * chair[nb.index] * ww;	    
	      if (fabs(c) > 0.001) {
		double eelec00; Coor grad00;
		elec(iab,c,rr2,dis[0],dis[1],dis[2],eelec00,grad00);
  		eelec += eelec00;
		grad[0] += grad00[0];
		grad[1] += grad00[1];
		grad[2] += grad00[2];
                if (!fixre) {	    
		  gradr[0] -= grad00[0];
		  gradr[1] -= grad00[1];
		  gradr[2] -= grad00[2];
                }
		elec(iab,c,g.plateaudissqinv,
		  rdis[0],rdis[1],rdis[2],eelec00,grad00);
  		eelec -= eelec00;
		grad[0] -= grad00[0];
		grad[1] -= grad00[1];
		grad[2] -= grad00[2];
                if (!fixre) {	    		
  		  gradr[0] += grad00[0];
		  gradr[1] += grad00[1];
		  gradr[2] += grad00[2];
                }
	      }	    	    
	    }
	    
	  }
	  else { //distance between 6-10 A 
            int proxmappos = int((dsq-g.proxlim)*proxspace+0.5);
	    int proxdsq = proxmap[proxmappos];
	    evdw += prox.prox[atomtype][atomtype2][2*proxdsq] * ww;
	    double gradf2 = prox.prox[atomtype][atomtype2][2*proxdsq+1] * ww;
	    
	    Coor dgrad = {
	      dis[0] * gradf2,
	      dis[1] * gradf2,
	      dis[2] * gradf2
	    };  
            grad[0] += dgrad[0];
	    grad[1] += dgrad[1];
	    grad[2] += dgrad[2];
            if (!fixre) {	    	    
              gradr[0] -= dgrad[0];
	      gradr[1] -= dgrad[1];
	      gradr[2] -= dgrad[2];
            }
	    if (chargenonzero) {
	      double c = charge0 * chair[nb.index] * ww;	    
	      if (fabs(c) > 0.001) {
	       
	       double rr2 = 1.0/dsq;
  	       dis[0] *= rr2; dis[1] *= rr2; dis[2] *= rr2;
	      
		double eelec00; Coor grad00;
		elec(iab,c,rr2,dis[0],dis[1],dis[2],eelec00,grad00);
  		eelec += eelec00;
		grad[0] += grad00[0];
		grad[1] += grad00[1];
		grad[2] += grad00[2];
                if (!fixre) {	    		
		  gradr[0] -= grad00[0];
		  gradr[1] -= grad00[1];
		  gradr[2] -= grad00[2];
                }
   	        double r = g.get_ratio(dsq);
		Coor rdis = {r*dis[0], r*dis[1], r*dis[2]};  
		
		elec(iab,c,g.plateaudissqinv,
		  rdis[0],rdis[1],rdis[2],eelec00,grad00);
  		eelec -= eelec00;
		grad[0] -= grad00[0];
		grad[1] -= grad00[1];
		grad[2] -= grad00[2];
                if (!fixre) {	    		
		  gradr[0] += grad00[0];
		  gradr[1] += grad00[1];
		  gradr[2] += grad00[2];
                }
	      }	    	    
	    }
	  }
	  
	}
      }
    }
  }
}

extern "C" void nonbon_grid_(
  const Grid *&g, const int &rigid, 
  const int &iab, const int &fixre,
  const Coor *xl, const Coor *xr,const Coor &pivotr,const Coor &tr,  
  const double *wel, const double *wer, const double *chail, const double *chair, const int *iacil, const int *iacir, 
  const int &natoml,const int &natomr,

const Parameters &rc, const Parameters &ac, const Parameters &emin, const Parameters &rmin2,
  const iParameters &ipon, const int &potshape, //ATTRACT params
  
  Coor *fl, double &evdw, double &eelec,
  
  Coor *fr, const double (&pm2)[3][3][3], double *deltar)
{
  evdw = 0;
  eelec = 0;
#ifdef TORQUEGRID  
  double rtorques[3][3];
  memset(rtorques,0,3*3*sizeof(double));
#endif  
  int counter2 = 0;
  for (int n = 0; n < natoml; n++) {
    int atomtype = iacil[n]-1;
    if (atomtype == -1) continue;
    counter2++;
    double charge0 = chail[n];
    double charge = charge0/felecsqrt;
    Coor &grad = fl[n];
#ifdef TORQUEGRID    
    trilin(xl[n],atomtype,charge0,charge,wel[n],*g,rigid,xr,wer,chair,iacir,
      rc,ac,emin,rmin2,ipon,potshape,iab,fixre,evdw,eelec,grad,fr,pm2,deltar,rtorques);
#else      
    trilin(xl[n],atomtype,charge0,charge,wel[n],*g,rigid,xr,wer,chair,iacir,
      rc,ac,emin,rmin2,ipon,potshape,iab,fixre,evdw,eelec,grad,fr,deltar);
#endif
  }

#ifdef TORQUEGRID      
  for (int j = 0; j < 3; j++) { //euler angle
    for (int k = 0; k < 3; k++) {//gradient
      for (int l = 0; l < 3; l++) { //coordinate
        deltar[j] += rtorques[l][k] * pm2[l][j][k];
      }
    }
  } 
#endif
}  
  
