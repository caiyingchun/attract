      subroutine monte(maxatom,totmaxatom,maxres,
     1 maxlig, maxdof, maxmode, maxmolpair,
     2 cartstatehandle,ministatehandle,
     3 nhm, nlig, 
     4 phi, ssi, rot, xa, ya, za, dlig, seed, label,
     5 gesa, energies, lablen)
c
c  variable metric minimizer (Harwell subroutine lib.  as in Jumna with modifications)
c     minimizes a single structure

      implicit none

c     Parameters
      integer cartstatehandle,ministatehandle
      integer maxlig,maxatom,totmaxatom,maxres,maxdof,maxmode,
     1 maxmolpair
      integer nlig, seed
      real *8 gesa, energies
      dimension energies(6)
      integer lablen
      character label
      dimension label(lablen)
      
      integer nhm
      dimension nhm(maxlig)
      real*8 phi, ssi, rot, dlig, xa, ya, za
      dimension phi(maxlig), ssi(maxlig), rot(maxlig)
      dimension dlig(maxmode, maxlig)
      dimension xa(maxlig), ya(maxlig), za(maxlig)

c     Local variables      
      real*8 enew, energies0
      real*8 rrot1,rrot2,rrot3,rrot4,sphi,sssi,srot
      dimension energies0(6)
c     integer dseed,i,ii,j,jj,k,kk,itr,nfun
      integer i,ii,j,jj,k,kk,itr,nfun
      integer itra, ieig, iori, fixre, gridmode,iscore,ivmax
      integer ju,jl,jb,nmodes
      integer iab,ijk,iaccept
      real*8 xnull
      real*8 scalecenter,scalemode,scalerot,rr
      real*8 rotmat,randrot,newrot,sum 
      real*8 xaa,delta,bol,pi,mctemp
      real*8 dseed
      dimension xaa(maxdof)
      dimension delta(maxdof)
      dimension rr(maxdof),randrot(0:9),rotmat(0:9),newrot(0:9)
      pi=3.141592654d0
      call ministate_f_monte(ministatehandle,
     1 iscore,ivmax,iori,itra,ieig,fixre,gridmode,mctemp,
     2 scalerot,scalecenter,scalemode)
     
c     always calculate only energies
      iab = 0

c
c  all variables without lig-hm
c
      jb=3*iori*(nlig-fixre)+3*itra*(nlig-fixre)
c  all variables including lig-hm
      nmodes = 0
      do 5 i=fixre, nlig
      nmodes = nmodes + nhm(i)
5     continue
      ju=jb+ieig*nmodes
c  only trans or ori
      jl=3*iori*(nlig-fixre)

      call ministate_calc_pairlist(ministatehandle,cartstatehandle)
     
      xnull=0.0d0
c     dseed=seed
      dseed=12345
      scalerot=pi*scalerot
      nfun=0
      itr=0
      if (iscore.eq.1) then
        iori = 1
        itra = 1
      endif
c intial energy evaluation      
c
      call energy(maxdof,maxmolpair,
     1 maxlig, maxatom,totmaxatom,maxmode,maxres,
     2 cartstatehandle,ministatehandle,
     3 iab,iori,itra,ieig,fixre,gridmode,
     4 phi,ssi,rot,xa,ya,za,dlig,seed,
     5 gesa,energies,delta)
       
      if (iscore.eq.2) then
        call print_struc(seed,label,gesa,energies,nlig,phi,ssi,rot,
     1  xa,ya,za,nhm,dlig,lablen)
      endif     
      
c   start Monte Carlo
      iaccept=1
      do 4000 ijk=1,ivmax 
c store old Euler angle, position and ligand and receptor coordinates
c
c phi,ssi,rot for first molecule are fixed!
      if(iaccept.eq.1) then
      if(iori.eq.1) then 
      do 118 i=1+fixre,nlig
      ii=3*(i-fixre-1)
      xaa(ii+1)=phi(i)
      xaa(ii+2)=ssi(i)
      xaa(ii+3)=rot(i)
  118 continue
      endif
      if(itra.eq.1) then
      do 122 i=1+fixre,nlig
      ii=jl+3*(i-fixre-1)
      xaa(ii+1)=xa(i)
      xaa(ii+2)=ya(i)
      xaa(ii+3)=za(i)
  122 continue
      endif
c if ligand flex is included store deformation factor in every mode in dlig(j)
 
      if(ieig.eq.1) then
      jj = 0
      do 130 j=1,nlig
      do 131 i=1,nhm(j)
      xaa(jb+jj+i)=dlig(i,j)
  131 continue
      jj = jj + nhm(j)
  130 continue
      endif
      endif
c old Cartesians are not stored!
c generate a total of ju random numbers
c     write(*,*)'random rr(1),rr(2)..', rr(1),rr(2),rr(3)
c make an Euler rotation
      if(iori.eq.1) then
        do 190 i=1+fixre,nlig
        ii=3*(i-fixre-1)
c        write(*,*)'before',phi(i),ssi(i),rot(i)
c       call crand(dseed,5,rr)
        call GGUBS(dseed,5,rr)
c       write(*,*)'rand',rr(1),rr(2),rr(3),rr(4),rr(5)
c       dseed = int(10000*rr(5))
c       write(*,*)'dseed', dseed
        rr(1)=rr(1)-0.5d0
        rr(2)=rr(2)-0.5d0
        rr(3)=rr(3)-0.5d0
        sum=1d0/sqrt(rr(1)**2+rr(2)**2+rr(3)**2)
        rr(1)=sum*rr(1)
        rr(2)=sum*rr(2)
        rr(3)=sum*rr(3)
        rr(4)= scalerot*(rr(4)-0.5d0) 
        rrot1=rr(1) 
        rrot2=rr(2) 
        rrot3=rr(3) 
        rrot4=rr(4) 
        sphi=phi(i)
        sssi=ssi(i)
        srot=rot(i)
c       write(*,*)'phi(i),ssi(i),rot(i)',i,phi(i),ssi(i),rot(i)
        call axisrot(rr,randrot)
        call euler2rotmat(phi(i),ssi(i),rot(i),rotmat)
        call matmult(rotmat,randrot,newrot)
        phi(i) = atan2(newrot(5),newrot(2))
        ssi(i) = acos(newrot(8))
        rot(i) = atan2(-newrot(7),-newrot(6))       
      if (abs(newrot(8)) >= 0.999999) then 
        phi(i) = 0.0d0
        ssi(i) = 0.0d0
        if (newrot(0) >= 0.999999) then
        rot(i) = 0.0d0
        else if (newrot(0) <= -0.999999) then
         rot(i) = pi
        else 
         rot(i) = acos(newrot(0))
       endif
      endif      
c        write(*,*)'new ii,c,phi,ssi,rot',i,ii,c,phi(i),ssi(i),rot(i)
  190    continue
      endif
c make a move in HM direction and update x, y(1,i) and y(2,i) and dlig(j)
c     call crand(dseed,ju+1,rr) 
      call GGUBS(dseed,ju+1,rr) 
c     dseed = int(10000*rr(ju+1))
      if(ieig.eq.1) then
      kk = 0
      do 1180 k=1,nlig
      do 1200 i=1,nhm(k)
      dlig(i,k)=xaa(i+jb+kk)+scalemode*(rr(i+jb+kk)-0.5d0)
 1200 continue
 1180 kk = kk + nhm(k)
      continue 
      endif
c make a translation of the ligand center
      if(itra.eq.1) then
      do 1220 i=1+fixre,nlig
      ii=jl+3*(i-fixre-1)
      xa(i)=xa(i)+scalecenter*(0.5d0-rr(ii+1))
      ya(i)=ya(i)+scalecenter*(0.5d0-rr(ii+2))
      za(i)=za(i)+scalecenter*(0.5d0-rr(ii+3))
c     write(*,*)'trans-step',i,ii,
c    1 0.5d0-rr(ii+1),0.5d0-rr(ii+2),0.5d0-rr(ii+3)
c    1 rr(ii+1),rr(ii+2),rr(ii+3),xaa(ii+1),xaa(ii+2),xaa(ii+3)
 1220 continue
      endif
      do 183 i=1,3
c      write (*,*),'lig',i,phi(i),ssi(i),rot(i),xa(i),ya(i),za(i)
  183 continue
      call energy(maxdof,maxmolpair,
     1 maxlig, maxatom,totmaxatom,maxmode,maxres,
     2 cartstatehandle,ministatehandle,
     3 iab,iori,itra,ieig,fixre,gridmode,
     4 phi,ssi,rot,xa,ya,za,dlig,seed,
     5 enew,energies0,delta)
c  new energy 
c      write (*,*),'Energy2', enew 
      bol=enew-gesa
      if (mctemp.eq.0) then
      bol=sign(1.0d0,-bol)
      else
      bol=exp(-bol/mctemp)
      endif
c      write(*,*)'exp(bol)',enew,gesa,enew-gesa,bol
c     call crand(dseed,2,rr)
      call GGUBS(dseed,2,rr)
c     dseed = int(10000*rr(2))
      if(bol.gt.rr(1)) then
c      write(*,*)'accept the step', bol, rr(1)
c     write(*,*)
c    1 'rrot1,rrot2,rrot3,rrot4,sphi,phi(i),sssi,ssi(i),srot,rot(i)',
c    2 rrot1,rrot2,rrot3,rrot4,sphi,phi(2),sssi,ssi(2),srot,rot(2)
      gesa=enew
      energies(:)=energies0(:)
      iaccept=1
      if (iscore.eq.2) then
        call print_struc(seed,label,gesa,energies,nlig,phi,ssi,rot,
     1  xa,ya,za,nhm,dlig,lablen)
      endif           
c overwrite old xaa variables, see above
      else
c do not overwrite xaa variables
c      write(*,*)' step rejected'
      iaccept=0
      if(iori.eq.1) then
      do 1118 i=1+fixre,nlig
      ii=3*(i-fixre-1)
      phi(i)=xaa(ii+1)
      ssi(i)=xaa(ii+2)
      rot(i)=xaa(ii+3)
 1118 continue
      endif
      if(itra.eq.1) then
      do 1122 i=1+fixre,nlig
      ii=jl+3*(i-fixre-1)
      xa(i)=xaa(ii+1)
      ya(i)=xaa(ii+2)
      za(i)=xaa(ii+3)
 1122 continue
      endif
c if ligand flex is included store deformation factor in every mode in dlig(j)

      if(ieig.eq.1) then
      jj = 0
      do 230 j=1,nlig
      do 231 i=1,nhm(j)
      dlig(i,j)=xaa(jb+jj+i)
  231 continue
      jj = jj + nhm(j)
  230 continue
      endif
      endif 
 4000 continue

c     Clean up
      call ministate_free_pairlist(ministatehandle)      
      end