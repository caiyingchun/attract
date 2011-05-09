#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <memory.h>

#ifdef GRADCHECK
const double d = 0.00001;
const double delta[][3] = {{d,0,0},{-d,0,0},{0,d,0},{0,-d,0},{0,0,d},{0,0,-d}};
#endif

#ifdef PRECOMPCHECK
double sigma_threshold = 10; //only consider voxels within 10 sigma
#endif

inline double transw(double weight) {
  if (weight > 0.5) return 0.5-(1-weight)*(1-weight);
  return weight*weight;
}

inline double gradw(double weight) {
  if (weight > 0.5) return 2 * (1-weight);
  return 2*weight;
}

struct Map {
  double emweight;      // weight of the energy of this map 
  double overlapmargin; //fraction of maximum overlap that must be reached;less will give an energy penalty

  double situs_origx, situs_origy, situs_origz, situs_width;
  float resolution;
  double sigma;
  double base;
  double fac;

  #ifdef PRECOMPCHECK
  double maxdist;
  double maxdistsq;
  #endif
  unsigned int dimx;
  unsigned int dimy;
  unsigned int dimz;
  double *densities;
  double *maxatoms;
  int nrmaxatoms;
  float *precomp_overlap;
  float *precomp_gradx;
  float *precomp_grady;
  float *precomp_gradz;
  double *weights;  
  double clash_threshold;
  double clash_weight;
};

Map maps[100];
int nrmaps = 0;

unsigned int nratoms; 

extern "C" void read_vol(char *vol_file, double *width, double *origx, double *origy, double *origz, unsigned *extx, unsigned *exty, unsigned *extz, double **phi);

#ifdef PRECOMPCHECK
typedef void (*Calcfunc) (Map &m, double dissq, double dx, double dy, double dz, double &totoverlap, int index, double &gradx, double &grady, double &gradz);

void calc_energy(Map &m, double dissq, double dx, double dy, double dz, double &totoverlap, int index, double &gradx, double &grady, double &gradz) {
  double density = m.densities[index];
  double overlap = density*pow(m.base, -dissq);
  totoverlap += overlap;
  /*
  double dis = sqrt(dissq);
  double grad = fac * -2 * dis * overlap;
  double fgrad = grad/dis;
  */
  double fgrad = m.fac * -2 * overlap;
  gradx += dx*fgrad;
  grady += dy*fgrad;
  gradz += dz*fgrad; 
}

inline void callfunc(
  Calcfunc func,
  
  int x, int y, int z, 
  double &ax, double &ay, double &az,

  Map &m, double dissq, double dx, double dy, double dz, double &totoverlap, int   index, double &gradx, double &grady, double &gradz
  ) 
  {

  func(m, dissq, dx,dy,dz,totoverlap, index,gradx,grady,gradz);  
}

void iterate_voxels(Calcfunc func, Map &m, double ax, double ay, double az, 
double &totoverlap, double &gradx, double &grady, double &gradz) 
{
 
  int px0 = floor(ax);
  if (px0 < 0) px0=0;  
  if (px0 >= m.dimx) px0 = m.dimx-1;
  double dx0 = ax - px0;
  double dx0sq = dx0*dx0;
  int px1 = ceil(ax);
  if (px1 < 0) px1=0;  
  if (px1 == px0) px1++;
  if (px1 >= m.dimx) px1 = m.dimx-1;
  double dx1 = px1-ax;
  double dx1sq = dx1*dx1;

  int py0 = floor(ay);
  if (py0 < 0) py0=0;  
  if (py0 >= m.dimy) py0 = m.dimy-1;
  double dy0 = ay - py0;
  double dy0sq = dy0*dy0;
  int py1 = ceil(ay);
  if (py1 < 0) py1=0;    
  if (py1 == py0) py1++;    
  if (py1 >= m.dimy) py1 = m.dimy-1;
  double dy1 = py1-ay;
  double dy1sq = dy1*dy1;

  int pz0 = floor(az);
  if (pz0 < 0) pz0=0;  
  if (pz0 >= m.dimz) pz0 = m.dimz-1;
  double dz0 = az - pz0;
  double dz0sq = dz0*dz0;
  int pz1 = ceil(az);
  if (pz1 < 0) pz1=0;     
  if (pz1 == pz0) pz1++;    
  if (pz1 >= m.dimz) pz1 = m.dimz-1;
  double dz1 = pz1-az;
  double dz1sq = dz1*dz1;

  int indx,indxy, indxyz;
  const int dimxy = m.dimx*m.dimy;
  const int &dimx = m.dimx;

  //positive x loop
  indx = px1;
  double dsqx = dx1sq;
  double dx = dx1;
  int x;
  for (x = px1; x < m.dimx; x++) {
    if (dsqx > m.maxdistsq) break;

    //positive y loop
    indxy = indx+dimx*py1;
    double dsqxy = dsqx + dy1sq;
    double dy = dy1;
    int y;
    for (y = py1; y < m.dimy; y++){
      if (dsqxy > m.maxdistsq) break;

      //positive z loop
      indxyz = indxy+dimxy*pz1;
      double dsqxyz = dsqxy + dz1sq;
      double dz = dz1;
      int z;
      for (z = pz1; z < m.dimz; z++){
        if (dsqxyz > m.maxdistsq) break;
	//dsqxyz is the actual distance squared

	callfunc(func,x,y,z,ax,ay,az, 
	  m, dsqxyz, dx,dy,dz,totoverlap, indxyz,gradx,grady,gradz);

	//increment positive z loop
	indxyz += dimxy;
	dsqxyz += 2 * dz + 1;
	dz++;
      }

      //negative z loop
      indxyz = indxy+dimxy*pz0;      
      dsqxyz = dsqxy + dz0sq;
      dz = dz0;
      for (z = pz0; z >= 0; z--){
        if (dsqxyz > m.maxdistsq) break;
	//dsqxyz is the actual distance squared

	callfunc(func,x,y,z,ax,ay,az, 	
	  m, dsqxyz, dx,dy,-dz,totoverlap, indxyz,gradx,grady,gradz);

	//increment negative z loop
	indxyz -= dimxy;
	dsqxyz += 2 * dz + 1;
	dz++;
      }

      //increment positive y loop
      indxy += dimx;
      dsqxy += 2 *dy + 1;
      dy++;	
    }


    //negative y loop
    indxy = indx+dimx*py0;    
    dsqxy = dsqx + dy0sq;
    dy = dy0;
    for (y = py0; y >= 0; y--){
      if (dsqxy > m.maxdistsq) break;

      //positive z loop
      indxyz = indxy+dimxy*pz1;
      double dsqxyz = dsqxy + dz1sq;
      double dz = dz1;
      int z;
      for (z = pz1; z < m.dimz; z++){
        if (dsqxyz > m.maxdistsq) break;
	//dsqxyz is the actual distance squared

	callfunc(func,x,y,z,ax,ay,az, 	
	  m, dsqxyz, dx,-dy,dz,totoverlap, indxyz,gradx,grady,gradz);

	//increment positive z loop
	indxyz += dimxy;
	dsqxyz += 2 * dz + 1;
	dz++;
      }

      //negative z loop
      indxyz = indxy+dimxy*pz0;
      dsqxyz = dsqxy + dz0sq;
      dz = dz0;
      for (z = pz0; z >= 0; z--){
        if (dsqxyz > m.maxdistsq) break;
	//dsqxyz is the actual distance squared

	callfunc(func,x,y,z,ax,ay,az, 	
	  m, dsqxyz, dx,-dy,-dz,totoverlap, indxyz,gradx,grady,gradz);

	//increment negative z loop
	indxyz -= dimxy;
	dsqxyz += 2 * dz + 1;
	dz++;
      }

      //increment negative y loop
      indxy -= dimx;
      dsqxy += 2 *dy + 1;
      dy++;	
    }


    //increment positive x loop
    indx++;
    dsqx += 2 *dx + 1;
    dx++;
  }

  //negative x loop
  indx = px0;  
  dsqx = dx0sq;
  dx = dx0;
  for (x = px0; x >= 0; x--) {
    if (dsqx > m.maxdistsq) break;

    //positive y loop
    indxy = indx+dimx*py1;        
    double dsqxy = dsqx + dy1sq;
    double dy = dy1;
    int y;
    for (y = py1; y < m.dimy; y++){
      if (dsqxy > m.maxdistsq) break;

      //positive z loop
      indxyz = indxy+dimxy*pz1;
      double dsqxyz = dsqxy + dz1sq;
      double dz = dz1;
      int z;
      for (z = pz1; z < m.dimz; z++){
        if (dsqxyz > m.maxdistsq) break;
	//dsqxyz is the actual distance squared

	callfunc(func,x,y,z,ax,ay,az, 	
	  m, dsqxyz, -dx,dy,dz,totoverlap, indxyz,gradx,grady,gradz);

	//increment positive z loop
	indxyz += dimxy;
	dsqxyz += 2 * dz + 1;
	dz++;
      }

      //negative z loop
      indxyz = indxy+dimxy*pz0;
      dsqxyz = dsqxy + dz0sq;
      dz = dz0;
      for (z = pz0; z >= 0; z--){
        if (dsqxyz > m.maxdistsq) break;
	//dsqxyz is the actual distance squared

	callfunc(func,x,y,z,ax,ay,az, 	
	  m, dsqxyz, -dx,dy,-dz,totoverlap, indxyz,gradx,grady,gradz);

	//increment negative z loop
	indxyz -= dimxy;
	dsqxyz += 2 * dz + 1;
	dz++;
      }

      //increment positive y loop
      indxy += dimx;
      dsqxy += 2 *dy + 1;
      dy++;	
    }


    //negative y loop
    indxy = indx+dimx*py0;    
    dsqxy = dsqx + dy0sq;
    dy = dy0;
    for (y = py0; y >= 0; y--){
      if (dsqxy > m.maxdistsq) break;

      //positive z loop
      indxyz = indxy+dimxy*pz1;
      double dsqxyz = dsqxy + dz1sq;
      double dz = dz1;
      int z;
      for (z = pz1; z < m.dimz; z++){
        if (dsqxyz > m.maxdistsq) break;
	//dsqxyz is the actual distance squared

	callfunc(func,x,y,z,ax,ay,az, 	
	  m, dsqxyz, -dx,-dy,dz,totoverlap, indxyz,gradx,grady,gradz);

	//increment positive z loop
	indxyz += dimxy;
	dsqxyz += 2 * dz + 1;
	dz++;
      }

      //negative z loop
      indxyz = indxy+dimxy*pz0;
      dsqxyz = dsqxy + dz0sq;
      dz = dz0;
      for (z = pz0; z >= 0; z--){
        if (dsqxyz > m.maxdistsq) break;
	//dsqxyz is the actual distance squared

	callfunc(func,x,y,z,ax,ay,az, 	
	  m, dsqxyz, -dx,-dy,-dz,totoverlap, indxyz,gradx,grady,gradz);

	//increment negative z loop
	indxyz -= dimxy;
	dsqxyz += 2 * dz + 1;
	dz++;
      }

      //increment negative y loop
      indxy -= dimx;
      dsqxy += 2 *dy + 1;
      dy++;	
    }


    //increment negative x loop
    indx--;    
    dsqx += 2 *dx + 1;
    dx++;
  }

}
#endif

inline void apply(Map &m, int indxyz, double cwxyz, double &overlap, double &gradx, double &grady, double &gradz, bool calc_clash, int sx, int sy, int sz) {
  if (!calc_clash) {
    overlap += cwxyz * m.precomp_overlap[indxyz];
    gradx += cwxyz * m.precomp_gradx[indxyz];
    grady += cwxyz * m.precomp_grady[indxyz];
    gradz += cwxyz * m.precomp_gradz[indxyz];
    m.weights[indxyz] += transw(cwxyz);	  
  }
  else {
    double w = m.weights[indxyz];
    double clash = w - m.clash_threshold;
    if (clash <= 0) return; 
    overlap += clash*clash;
    double cgrad = 2*clash*gradw(cwxyz);
    gradx -= sx * cgrad;
    grady -= sy * cgrad;
    gradz -= sz * cgrad;
  }
}

inline void trilin(Map &m, double ax, double ay, double az, double &overlap, double &gradx, double &grady, double &gradz, bool calc_clash) {
  double wx1=0,wy1=0,wz1=0;
  
  int px0 = floor(ax);  
  int px1 = ceil(ax);
  if (px1 < 0 || px0 >= m.dimx) return;
  if (px0 < 0) wx1 = 0;
  else if (px1 >= m.dimx) wx1 = 1;
  else wx1 = ax - px0;

  int py0 = floor(ay);  
  int py1 = ceil(ay);
  if (py1 < 0 || py0 >= m.dimy) return;
  if (py0 < 0) wy1 = 0;
  else if (py1 >= m.dimy) wy1 = 1;
  else wy1 = ay - py0;

  int pz0 = floor(az);  
  int pz1 = ceil(az);
  if (pz1 < 0 || pz0 >= m.dimz) return;
  if (pz0 < 0) wz1 = 0;
  else if (pz1 >= m.dimz) wz1 = 1;
  else wz1 = az - pz0;
  
  double wx0 = 1-wx1, wy0 = 1-wy1, wz0 = 1-wz1;
  double dimxy = m.dimx*m.dimy;
  double cwx, cwxy, cwxyz, dsqx, dsqxy, dsqxyz;
  int indx,indxy,indxyz; 
  if (wx0 > 0) {
    cwx = wx0;
    dsqx = wx1*wx1;
    indx = px0;
    if (wy0 > 0){
      cwxy = cwx * wy0;
      dsqxy = dsqx + wy1*wy1;
      indxy = indx + m.dimx*py0;      
      if (wz0 > 0) {
        cwxyz = cwxy * wz0;
	dsqxyz = dsqxy + wz1*wz1;
        indxyz = indxy + dimxy*pz0;      
        apply(m,indxyz,cwxyz,overlap,gradx,grady,gradz,calc_clash,-1,-1,-1);
      }
      if (wz1 > 0) {
        cwxyz = cwxy * wz1;
	dsqxyz = dsqxy + wz0*wz0;
        indxyz = indxy + dimxy*pz1;      
        apply(m,indxyz,cwxyz,overlap,gradx,grady,gradz,calc_clash,-1,-1,1);
      }
    }
    if (wy1 > 0) {
      cwxy = cwx * wy1;
      dsqxy = dsqx + wy0*wy0;
      indxy = indx + m.dimx*py1;      
      if (wz0 > 0) {
        cwxyz = cwxy * wz0;
	dsqxyz = dsqxy + wz1*wz1;
        indxyz = indxy + dimxy*pz0;      
        apply(m,indxyz,cwxyz,overlap,gradx,grady,gradz,calc_clash,-1,1,-1);
      }
      if (wz1 > 0) {
        cwxyz = cwxy * wz1;
	dsqxyz = dsqxy + wz0*wz0;
        indxyz = indxy + dimxy*pz1;      
        apply(m,indxyz,cwxyz,overlap,gradx,grady,gradz,calc_clash,-1,1,1);
      }
    }
  }
  if (wx1 > 0) {
    cwx = wx1;
    dsqx = wx0*wx0;
    indx = px1;
    if (wy0 > 0){
      cwxy = cwx * wy0;
      dsqxy = dsqx + wy1*wy1;
      indxy = indx + m.dimx*py0;      
      if (wz0 > 0) {
        cwxyz = cwxy * wz0;
	dsqxyz = dsqxy + wz1*wz1;
        indxyz = indxy + dimxy*pz0;      
        apply(m,indxyz,cwxyz,overlap,gradx,grady,gradz,calc_clash,1,-1,-1);
      }
      if (wz1 > 0) {
        cwxyz = cwxy * wz1;
	dsqxyz = dsqxy + wz0*wz0;
        indxyz = indxy + dimxy*pz1;      
        apply(m,indxyz,cwxyz,overlap,gradx,grady,gradz,calc_clash,1,-1,1);
      }
    }
    if (wy1 > 0) {
      cwxy = cwx * wy1;
      dsqxy = dsqx + wy0*wy0;
      indxy = indx + m.dimx*py1;      
      if (wz0 > 0) {
        cwxyz = cwxy * wz0;
	dsqxyz = dsqxy + wz1*wz1;
        indxyz = indxy + dimxy*pz0;      
        apply(m,indxyz,cwxyz,overlap,gradx,grady,gradz,calc_clash,1,1,-1);
      }
      if (wz1 > 0) {
        cwxyz = cwxy * wz1;
	dsqxyz = dsqxy + wz0*wz0;
        indxyz = indxy + dimxy*pz1;      
        apply(m,indxyz,cwxyz,overlap,gradx,grady,gradz,calc_clash,1,1,1);
      }
    }
  }
}


extern "C" void read_densitymaps_(char *densitymapsfile0, int len_densitymapsfile) {
  char buf[2000];
  
  char *densitymapsfile = new char[len_densitymapsfile];
  memcpy(densitymapsfile, densitymapsfile0, len_densitymapsfile);
  
  FILE *fil = fopen(densitymapsfile, "r");
  if (fil == NULL) {
    fprintf(stderr, "ERROR: file %s does not exist\n", densitymapsfile);
    exit(1);
  }
  
  while (!feof(fil)) {
    char densitymap[2000];
    char densitymax[2000];
    char precomp[2000];
    float resolution_value;
    if (!fgets(buf, 2000, fil)) break;
    if (buf[0] == '#') {
      buf[0] = ' ';
      continue;
    }
    float overlapmargin, emweight, clash_threshold, clash_weight;
    int read = sscanf(buf, "%f %s %s %f %f %s %f %f", &resolution_value, densitymap, densitymax, &overlapmargin, &emweight, precomp, &clash_threshold, &clash_weight);
    if (read <= 1) continue;
    if (read != 8) {
      fprintf(stderr, "Reading error in %s:\n%s\n", densitymapsfile, buf);
      exit(1);
    }
    Map &m = maps[nrmaps];
    m.overlapmargin = overlapmargin;
    m.emweight = emweight;
    m.resolution = resolution_value;
    m.sigma = 0.5 * m.resolution / sqrt(3);
    double sigmafactor = 1/(2*m.sigma*m.sigma);
    m.fac = (sigmafactor*sigmafactor)/(sigmafactor+sigmafactor);
    m.base = exp(m.fac);

    read_vol(densitymap, &m.situs_width, &m.situs_origx,  &m.situs_origy,&m.situs_origz,&m.dimx,&m.dimy,&m.dimz,&m.densities);        

    const int maxmaxatoms = 100000;
    double maxatoms[maxmaxatoms];
    FILE *fil2 = fopen(densitymax, "r");
    if (fil2 == NULL) {
      fprintf(stderr, "ERROR: file %s does not exist\n", densitymax);
      exit(1);
    }    
    int nrmaxatoms = 0;
    while(!feof(fil2)) {
      if (!fgets(buf, 2000, fil2)) break;
      float max;
      if (!sscanf(buf, "%*s %*d %*s %f", &max)) break;
      if (nrmaxatoms == maxmaxatoms) {
        fprintf(stderr, "%s contains more than %d atoms\n", densitymax, maxmaxatoms);
	exit(1);
      }
      maxatoms[nrmaxatoms] = max;
      nrmaxatoms++;      
    }
    fclose(fil2);
    m.maxatoms = new double[nrmaxatoms];
    memcpy(m.maxatoms, maxatoms, nrmaxatoms*sizeof(double));
    m.nrmaxatoms = nrmaxatoms;
    
    #ifdef PRECOMPCHECK
    double voxels_per_sigma = m.sigma / m.situs_width;
    m.maxdist = sigma_threshold * voxels_per_sigma;
    m.maxdistsq = m.maxdist * m.maxdist;    
    #endif
    
    fil2 = fopen(precomp, "r");
    if (fil2 == NULL) {
      fprintf(stderr, "ERROR: file %s does not exist\n", precomp);
      exit(1);
    }    
    int gridsize = m.dimx * m.dimy * m.dimz;
    m.precomp_overlap = new float[gridsize];
    m.precomp_gradx = new float[gridsize];
    m.precomp_grady = new float[gridsize];
    m.precomp_gradz = new float[gridsize];
    m.weights = new double[gridsize];
    
    for (int n = 0; n < gridsize; n++) {
      bool ok = 0;
      do {
        if (!fgets(buf, 2000, fil2)) break;    
        int read = sscanf(buf, "%f %f %f %f", 
	  &m.precomp_overlap[n], &m.precomp_gradx[n], &m.precomp_grady[n], &m.precomp_gradz[n]);
	if (read != 4) break;
	ok = 1;
      } while (0);
      if (!ok){
        fprintf(stderr, "ERROR: Reading error in precomputed file %s, line %d\n", precomp,n+1);
        exit(1);
      }
    }    
    
    
    fclose(fil2);
    
    m.clash_threshold = clash_threshold;
    m.clash_weight = clash_weight;
    
    nrmaps++;
  }
  fclose(fil);
  delete[] densitymapsfile;
}

double emenergy (Map &m, int &nratoms, double *atoms, double *forces, int &update_forces, double &totgradx, double &totgrady, double &totgradz) {  
  double energy = 0;
  double clashenergy = 0;
  if (nratoms != m.nrmaxatoms) {
    int mapnr = &m - maps+1;
    fprintf(stderr, "ERROR: Electron density map %d: number of atoms in PDB file (%d) is not equal in the number of atoms in the maximum density file (%d)\n", mapnr, nratoms, m.nrmaxatoms);
    exit(0);
  }

  totgradx = 0; totgrady = 0; totgradz = 0;
  int gridsize = m.dimx*m.dimy*m.dimz;
  memset(m.weights, 0, gridsize * sizeof(double));
  int n;    
  for (n=0;n<nratoms;n++) {
    double ax = (atoms[6*n] - m.situs_origx)/m.situs_width;    
    double ay = (atoms[6*n+2] - m.situs_origy)/m.situs_width;
    double az = (atoms[6*n+4] - m.situs_origz)/m.situs_width;
    double totoverlap = 0;
    double gradx = 0, grady = 0, gradz = 0;

    trilin(m,ax,ay,az,totoverlap, gradx,grady,gradz, 0);

    #ifdef PRECOMPCHECK
    double totoverlap0 = 0;
    double gradx0 = 0, grady0 = 0, gradz0 = 0;    
    iterate_voxels(calc_energy, m, ax,ay,az,totoverlap0,gradx0,grady0,gradz0);
  
    printf("atom %d overlap %.7g %.7g gradx %.7g %.7g grady %.7g %.7g gradz %.7g %.7g\n", n+1, totoverlap, totoverlap0, gradx, gradx0, grady, grady0, gradz, gradz0); 
    #endif
    
    double max_overlap = m.maxatoms[n];  
    double gradscale = 2*m.emweight/(-max_overlap*m.situs_width);
    totoverlap /= max_overlap;
    double curr_energy = 0;
    if (totoverlap < m.overlapmargin) {
      double lack = m.overlapmargin - totoverlap;
      curr_energy = m.emweight * lack*lack;
      energy += curr_energy;
      double currgradscale = lack * gradscale;
      if (update_forces == 1) {
        gradx *= currgradscale;
        grady *= currgradscale;
        gradz *= currgradscale;
	totgradx += gradx; totgrady += grady; totgradz += gradz;	
        forces[3*n] += gradx;
        forces[3*n+1] += grady;
        forces[3*n+2] += gradz;
      }
    }
  }
  if (m.clash_threshold > 0 && m.clash_weight > 0) {
    for (n=0;n<nratoms;n++) {
      double ax = (atoms[6*n] - m.situs_origx)/m.situs_width;    
      double ay = (atoms[6*n+2] - m.situs_origy)/m.situs_width;
      double az = (atoms[6*n+4] - m.situs_origz)/m.situs_width;
      double totclash = 0;
      double gradx = 0, grady = 0, gradz = 0;

      trilin(m,ax,ay,az,totclash, gradx,grady,gradz, 1);

      clashenergy += totclash * m.clash_weight;
      if (update_forces == 1) {
        gradx *= m.clash_weight;
        grady *= m.clash_weight;
        gradz *= m.clash_weight;
        totgradx += gradx; totgrady += grady; totgradz += gradz;	
        forces[3*n] += gradx;
        forces[3*n+1] += grady;
        forces[3*n+2] += gradz;
      }
    }
  }
  if (m.clash_threshold > 0 && m.clash_weight > 0) {
    printf("EM energy: %d %.3f %.3f\n", &m-maps+1, energy, clashenergy);  
    energy += clashenergy;
  }
  else {
    printf("EM energy: %d %.3f\n", &m-maps+1, energy);  
  }
  return energy;  
}


extern "C" void emenergy_(double &energy, int &nratoms, double *atoms, double *forces, int &update_forces) {  

  energy = 0;
  for (int m = 0; m < nrmaps; m++) {

#ifdef GRADCHECK
  
    double tgradx,tgrady,tgradz, tgradx0,tgrady0,tgradz0;
    double denergy = emenergy(maps[m], nratoms, atoms, forces, update_forces, tgradx,tgrady,tgradz);
    energy += denergy;
    
    double *datoms = new double[6*nratoms];
    memcpy(datoms, atoms, 6*nratoms*sizeof(double));
    double energy0;
    //printf("tgradx %.6g tgrady %.6g tgradz %.6g\n", tgradx,tgrady,tgradz);
    for (int i=0;i<6;i++) {
      memcpy(datoms, atoms, 6*nratoms*sizeof(double));
      for (int n=0;n<nratoms;n++) {
        datoms[6*n] += delta[i][0];
        datoms[6*n+2] += delta[i][1];
        datoms[6*n+4] += delta[i][2];
      }    
      double grad = 0;
      if (delta[i][0] != 0) grad = tgradx;
      if (delta[i][1] != 0) grad = tgrady;
      if (delta[i][2] != 0) grad = tgradz;
      int no_update = 0;
      energy0 = emenergy(maps[m], nratoms, datoms, forces, no_update, tgradx0,tgrady0,tgradz0);
      printf("trans%d %.3f %.6g %.6g %.6g\n", i, energy0, (energy0-denergy)/d, grad, grad/((energy0-denergy)/d));
    }
    printf("\n");
  }
}

#else
    //Normal execution
    
    double dumx,dumy,dumz;
    double denergy = emenergy(maps[m], nratoms, atoms, forces, update_forces,dumx,dumy,dumz);
    energy += denergy;
  }
  #ifdef PRECOMPCHECK
  exit(0);
  #endif
}

#endif
