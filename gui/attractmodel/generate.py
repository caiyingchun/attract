import os

#TODO: fix ATTRACT-EM interface
#TODO: fix pqreduce XXXX hack
#TODO: add check between p.modes_file and p.nr_modes
#TODO: add check that either all interactions are grid-accelerated, or none are
#  (and an option to disable this check; for this, adapt --rcut iteration as well)
#TODO: a script to add energies back to deredundant output
#TODO: add grid alphabet to data model
#TODO: non-protein molecules
#TODO: decide upon PDB code, chain select

generator_version = "0.1"

parameterfiledict = {
  "ATTRACT" :  "$ATTRACTDIR/../parmw.par",
  "OPLSX" : "$ATTRACTDIR/../allatom/allatom.par",
}

topologyfiledict = {
  ("Protein", "OPLSX"): "$ATTRACTDIR/../allatom/topallhdg5.3.pro",
}

transfiledict = {
  ("Protein", "OPLSX"): "$ATTRACTDIR/../allatom/oplsx.trans",
}

def generate(m):    
  iattract_hybrid = False #are we doing the docking with ATTRACT forcefield, but iATTRACT refinement with iATTRACT ?
  if (m.iattract and m.forcefield != "OPLSX"): iattract_hybrid = True
   
  ret = "#!/bin/bash -i\n"
  cleanupfiles = []
  if (m.header is not None):
    ret += m.header + "\n\n"
  if m.annotation is not None:
    for l in m.annotation.split("\n"):
      ret += "###" + l + "\n"
  ret += "### Generated by ATTRACT shell script generator version %s\n\n" % generator_version    
  
  ret += 'trap "kill -- -$BASHPID" 2\n'  
  ret += "$ATTRACTDIR/shm-clean\n\n"
  results = "result.dat result.pdb result.lrmsd result.irmsd result.fnat"
  ret += "rm -rf %s >& /dev/null\n" % results

  partnercode = ""
  filenames = []
  ensemble_lists = []
  collect_ensemble_lists = []
  iattract_filenames = []
  iattract_ensemble_lists = []
  reduce_any = False
  pdbnames = []
  pdbnames3 = set()
  for pnr,p in enumerate(m.partners):
    assert p.code is None #TODO: not implemented
    #TODO: select chain
    ensemble_list = None
    ensemble_list_iattract = None
    if p.ensemble_list is not None: ensemble_list = p.ensemble_list.name
    collect_ensemble_list = None    
    if p.collect_ensemble_list is not None: collect_ensemble_list = p.collect_ensemble_list.name        
    if p.is_reduced == False:
      if not p.ensemble:
        if reduce_any == False:
          partnercode += """
echo '**************************************************************'
echo 'Reduce partner PDBs...'
echo '**************************************************************'
"""
  
          reduce_any = True
        pdbname = p.pdbfile.name
        pdbname2 = os.path.split(pdbname)[1]
        if pdbname not in pdbnames: 
          if pdbname2.count(".") > 1:
            raise ValueError("The 'reduce' program does not support PDB files with double extension: '%s'" % pdbname2)
          pdbname3 = os.path.splitext(pdbname2)[0]
          pdbname3_0 = pdbname3
          pcount = 0
          while pdbname3 in pdbnames3:
            pcount += 1
            pdbname3 = pdbname3_0 + "-" + str(pcount)
          pdbnames3.add(pdbname3)
          pdbname4 = pdbname3 + ".pdb"
          if pdbname4 != pdbname:
            partnercode += "cat %s > %s\n" % (pdbname, pdbname4)          
          if m.forcefield == "ATTRACT":
            pdbname_reduced = pdbname3 + "r.pdb"
            partnercode += "$ATTRACTDIR/reduce %s > /dev/null\n" % pdbname4            
          if m.forcefield == "OPLSX" or iattract_hybrid:
            pdbname_aa = pdbname3 + "-aa.pdb"            
            topfile = topologyfiledict[p.moleculetype, "OPLSX"]
            transfile = transfiledict[p.moleculetype, "OPLSX"]
            partnercode += "python $ATTRACTDIR/../allatom/pqreduce.py %s %s %s > /dev/null\n" % (pdbname4, transfile, topfile)
            partnercode += "grep -v XXXX %s > /tmp/pqtemp; mv -f /tmp/pqtemp %s\n" % (pdbname_aa, pdbname_aa) ###HACK
            if iattract_hybrid: 
              iattract_filenames.append(pdbname_aa)
            else:
              pdbname_reduced = pdbname_aa
          pdbnames.append(pdbname)
        else:
          pdbname_reduced = filenames[pdbnames.index(pdbname)]
        filenames.append(pdbname_reduced)
    else:        
      filenames.append(p.pdbfile.name) 
    if p.ensemble:
      d = "partner%d-ensemble" % (pnr+1)
      partnercode += "rm -rf %s\n" % d
      partnercode += "mkdir %s\n" % d
      listens = d + ".list"
      ensemble_list_iattract = d + "-iattract.list"
      if not p.is_reduced:
        listensr = d + "-r.list"
      else:
        listensr = listens
      dd = d + "/model"
      partnercode += "$ATTRACTTOOLS/splitmodel %s %s %d > %s\n" % (p.pdbfile.name, dd, p.ensemble_size, listens)
      if not p.is_reduced:
        partnercode += "cat /dev/null > %s\n" % listensr
      if iattract_hybrid:
	partnercode += "cat /dev/null > %s\n" % ensemble_list_iattract
      if not p.is_reduced:
        if reduce_any == False:
            partnercode += """
echo '**************************************************************'
echo 'Reduce partner PDBs...'
echo '**************************************************************'
"""      
            reduce_any = True        
      for mnr in range(p.ensemble_size):
        mname1 = dd + "-" + str(mnr+1) + ".pdb"
        if not p.is_reduced:
          if m.forcefield == "ATTRACT":
            mname2 = dd + "-" + str(mnr+1) + "r.pdb"
            partnercode += "$ATTRACTDIR/reduce %s > /dev/null\n" % mname1
          if m.forcefield == "OPLSX" or iattract_hybrid:
            mname2a = dd + "-" + str(mnr+1) + "-aa.pdb"
            topfile = topologyfiledict[p.moleculetype, "OPLSX"]
            transfile = transfiledict[p.moleculetype, "OPLSX"]            
            partnercode += "python $ATTRACTDIR/../allatom/pqreduce.py %s %s %s > /dev/null\n" % (mname1, transfile, topfile)
            partnercode += "grep -v XXXX %s > /tmp/pqtemp; mv -f /tmp/pqtemp %s\n" % (mname2a, mname2a) ###HACK
            if m.forcefield == "OPLSX": 
              mname2 = mname2a
            elif iattract_hybrid:
              partnercode += "echo %s >> %s\n" % (mname2a, ensemble_list_iattract)              
          partnercode += "echo %s >> %s\n" % (mname2, listensr)  
          if mnr == 0: 
            filenames.append(mname2)
            if iattract_hybrid: iattract_filenames.append(mname2a)
            p.collect_pdb.name = mname1          
      ensemble_list = listensr      
      if p.collect_pdb is not None:        
        if collect_ensemble_list is None: collect_ensemble_list = listens      
    ensemble_lists.append(ensemble_list)
    if iattract_hybrid:
      iattract_ensemble_lists.append(ensemble_list_iattract)
    collect_ensemble_lists.append(collect_ensemble_list)      
  if reduce_any: partnercode += "\n"

  if m.iattract and not iattract_hybrid:
    iattract_filenames = filenames
    iattract_ensemble_lists = ensemble_lists

  modes_any = any((p.modes_file or p.generate_modes for p in m.partners))
  unreduced_modes_any = any((p.aa_modes_file or p.generate_modes for p in m.partners))
  if modes_any:
    partnercode += """
echo '**************************************************************'
echo 'Assemble modes file...'
echo '**************************************************************'
cat /dev/null > hm-all.dat
""" 
    if iattract_hybrid:
      partnercode += "cat /dev/null > hm-all-iattract.dat\n"
    if unreduced_modes_any:
      hm_all_unreduced = "hm-all-unreduced.dat"
      partnercode += "cat /dev/null > hm-all-unreduced.dat\n"
    else: 
      hm_all_unreduced = "hm-all.dat"
    partnercode += "\n"  
    for pnr,p in enumerate(m.partners):
      modes_file_name = None
      if p.modes_file is not None: modes_file_name = p.modes_file.name
      unreduced_modes_file_name = None
      if p.aa_modes_file is not None: unreduced_modes_file_name = p.aa_modes_file.name
      if p.generate_modes: 
        modes_file_name = "partner%d-hm.dat" % (pnr+1)
        partnercode += "python $ATTRACTTOOLS/modes.py %s > %s\n" % (filenames[pnr], modes_file_name)
        if iattract_hybrid:
          partnercode += "python $ATTRACTTOOLS/modes.py %s %d >> hm-all-iattract.dat\n" % (iattract_filenames[pnr], p.nr_modes)
        if p.collect_pdb is not None:
          unreduced_modes_file_name = "partner%d-hm-unreduced.dat" % (pnr+1)
          partnercode += "python $ATTRACTTOOLS/modes.py %s > %s\n" % (p.collect_pdb.name, unreduced_modes_file_name)
      if p.moleculetype != "Protein": raise Exception #TODO: not yet implemented
      if p.nr_modes == 0 or modes_file_name is None:
        partnercode += "echo 0 >> hm-all.dat\n"        
        if iattract_hybrid:
          partnercode += "echo 0 >> hm-all-iattract.dat\n"
        if unreduced_modes_any:
          partnercode += "echo 0 >> hm-all-unreduced.dat\n"
      elif p.nr_modes == 10:
        partnercode += "cat %s >> hm-all.dat\n" % modes_file_name        
        if not m.demode:
          if unreduced_modes_any:
            mf = modes_file_name
            if unreduced_modes_file_name is not None: mf = unreduced_modes_file_name
            partnercode += "cat %s >> hm-all-unreduced.dat\n" % mf
      else:
        partnercode += "#select first %d modes...\n" % (p.nr_modes)
        partnercode += "awk 'NF == 2 && $1 == %d{exit}{print $0}' %s >> hm-all.dat\n" % \
         (p.nr_modes+1, modes_file_name)
        if unreduced_modes_any:
          mf = modes_file_name
          if unreduced_modes_file_name is not None: mf = unreduced_modes_file_name
          partnercode += "awk 'NF == 2 && $1 == %d{exit}{print $0}' %s >> hm-all-unreduced.dat\n" % \
          (p.nr_modes+1, mf)         
    partnercode += "\n"  
  
  if m.iattract:
    iattract_modesfile = "hm-all.dat"
    if iattract_hybrid:
      iattract_modesfile = "hm-all-iattract.dat"    
  
  if len(m.partners) == 2:
    partnerfiles = " ".join(filenames)
  else:
    partnerfiles = "partners.pdb"
    partnercode += """
echo '**************************************************************'
echo 'Concatenate partner PDBs...'
echo '**************************************************************'
echo %d > partners.pdb
""" % len(m.partners)
    for f in filenames:
      partnercode += "grep ATOM %s >> partners.pdb\n" % f
      partnercode += "echo TER >> partners.pdb\n"
          
  ret += """
#name of the run
name=%s 
""" % m.runname
  ffpar = parameterfiledict[m.forcefield]
  params = "\"" + ffpar + " " + partnerfiles
  scoreparams = params + " --score --fix-receptor"
  gridparams = ""
  if m.fix_receptor: params += " --fix-receptor" 
  if modes_any: 
    ps = " --modes hm-all.dat"
    params += ps
    scoreparams += ps
  gridfiles = {}
  ret_shm = ""
  for g in m.grids:
    if g.gridfile is not None: 
      v = g.gridfile
      if m.np > 1:
        v2 = v +"header"      
        cleanupfiles.append(v2)
        tq = "-torque" if g.torque else ""
        ret_shm = "\n#Load %s grid into memory\n" % (g.gridname)
        ret_shm += "$ATTRACTDIR/shm-grid%s %s %s\n" % (tq, v, v2)
        v = v2
    else:  
      gheader = ""
      if m.np > 1: gheader = "header"
      extension = ".tgrid" if g.torque else ".grid"
      v = g.gridname.strip() + extension + gheader
    gridfiles[g.gridname.strip()] = (v, g.torque)
  grid_used = {}
  ens_any = False
  for pnr,p in enumerate(m.partners):
    if p.gridname is not None:
      v, is_torque = gridfiles[p.gridname.strip()]
      if v in grid_used: 
        v = grid_used[v]
      else:
        grid_used[v] = pnr+1
      gridoption = "--torquegrid" if is_torque else "--grid"
      gridparams += " %s %d %s" % (gridoption, pnr+1, str(v))
    if ensemble_lists[pnr] is not None:
      ps = " --ens %d %s" % (pnr+1, ensemble_lists[pnr])
      params += ps
      scoreparams += ps
      ens_any = True
  if m.ghost:
    params += " --ghost"
  if m.gravity:
    params += " --gravity %d" % m.gravity
  if m.rstk != 0.01:
    params += " --rstk %s" % str(m.rstk)
  if m.dielec == "cdie": 
    ps = " --cdie"
    params += ps  
    scoreparams += ps  
  if m.epsilon != 15: 
    ps = " --epsilon %s" % (str(m.epsilon))
    params += ps
    scoreparams += ps  
  if m.restraints_file is not None:
    ps = " --rest %s" % (str(m.restraints_file.name))
    params += ps
  if m.restraints_score_file is not None:
    ps = " --rest %s" % (str(m.rest_score_file.name))
    scoreparams += ps
        
  for sym in m.symmetries:        
    if sym.symmetry_axis is None: #distance-restrained symmetry      
      symcode = len(sym.partners)
      if sym.symmetry == "Dx": symcode = -4
      partners = " ".join([str(v) for v in sym.partners])
      params += " --sym %d %s" % (symcode, partners)
    else: #generative symmetry
      symcode = sym.symmetry_fold
      partner = sym.partners[0]
      s = sym.symmetry_axis
      sym_axis = "%.3f %.3f %.3f" % (s.x, s.y, s.z)
      s = sym.symmetry_origin
      sym_origin = "%.3f %.3f %.3f" % (s.x, s.y, s.z)      
      params += " --axsym %d %d %s %s" % (partner, symcode, sym_axis, sym_origin)
  params += "\""
  scoreparams += "\""
  ret += """
#docking parameters
params=%s
scoreparams=%s
"""  % (params, scoreparams)
  if len(gridparams):
    ret += """
#grid parameters
gridparams="%s"
"""  % gridparams
  
  if m.np > 1:
    if m.jobsize == 0:
      parals = "--np %d --chunks %d" % (m.np, m.np)
    else:
      parals = "--np %d --jobsize %d" % (m.np, m.jobsize)
    ret += """
#parallelization parameters
parals="%s"
"""  % (parals)
    
  ret += "if [ 1 -eq 1 ]; then ### move and change to disable parts of the protocol\n"  
  ret += partnercode
  
  ret += ret_shm
  if m.search == "syst" or m.search == "custom":
    if m.search == "syst" or m.start_structures_file is None:
      ret += """
echo '**************************************************************'
echo 'Generate starting structures...'
echo '**************************************************************'
"""
      rotfil = "$ATTRACTDIR/../rotation.dat"
      if m.rotations_file is not None:
        rotfil = m.rotations_file.name
      if rotfil != "rotation.dat":
        ret += "cat %s > rotation.dat\n" % rotfil
      if m.translations_file is not None:
        transfil = m.translations_file.name
        if transfil != "translate.dat":
          ret += "cat %s > translate.dat\n" % transfil
      else:
        ret += "$ATTRACTDIR/translate %s %s > translate.dat\n" % \
         (filenames[0], filenames[1])
      ret += "$ATTRACTDIR/systsearch > systsearch.dat\n"
      ret += "start=systsearch.dat\n"
      start = "systsearch.dat"
    else:
      ret += """
#starting structures
start=%s 
""" % m.start_structures_file.name
      start = m.start_structures_file.name
  elif m.search == "random":
    fixre = ""
    if m.fix_receptor: fixre = " --fix-receptor"
    ret += "python $ATTRACTTOOLS/randsearch.py %d %d%s > randsearch.dat\n" % \
     (len(m.partners), m.structures, fixre)
    ret += "start=randsearch.dat\n"    
    start = "randsearch.dat"
  else:
    raise Exception("Unknown value")
  ret += "\n"  
  
  inp = "$start"
  if any((p.ensemblize for p in m.partners if p.ensemblize not in (None,"custom"))):
    start0 = start
    ret += """
echo '**************************************************************'
echo 'ensemble search:' 
echo ' add ensemble conformations to the starting structures'
echo '**************************************************************'
"""
    for pnr, p in enumerate(m.partners):
      if p.ensemblize in (None, "custom"): continue
      if p.ensemblize == "all":
        ret += """
echo '**************************************************************'
echo 'multiply the number of structures by the ensemble for ligand %d'
echo '**************************************************************'
""" % (pnr+1)
      elif p.ensemblize == "random":
        ret += """
echo '**************************************************************'
echo 'random ensemble conformation in ligand %d for each starting structure'
echo '**************************************************************'
""" % (pnr+1)
      else: 
        raise ValueError(p.ensemblize)
      if start == start0 and start not in ("systsearch.dat", "randsearch.dat"):
        start2 = "start-ens%d.dat" % (pnr+1)
      else:
        start2 = os.path.splitext(start)[0] + "-ens%d.dat" % (pnr+1)
      ret += "python $ATTRACTTOOLS/ensemblize.py %s %d %d %s  > %s\n" % \
       (start, p.ensemble_size, pnr+1, p.ensemblize, start2)
      start = start2
      inp = start 
    
  for g in m.grids:
    if g.gridfile is not None: continue
    gridfile = gridfiles[g.gridname.strip()][0]
    if gridfile not in grid_used: continue
    ret += """
echo '**************************************************************'
echo 'calculate %s grid'
echo '**************************************************************'
""" % g.gridname
    partner = grid_used[gridfile]-1
    f = filenames[partner]
    f0 = os.path.splitext(f)[0]
    tomp = ""
    if g.torque: tomp += "-torque"
    if g.omp: tomp += "-omp"
    tail = ""
    if m.np > 1: 
      tail += " --shm"
    moleculetype = m.partners[partner].moleculetype
    #TODO: support non-protein grids
    if (moleculetype, m.forcefield) in transfiledict: 
      #TODO: alphabet might be overridden...      
      tail += " --alphabet %s" % transfiledict[moleculetype, m.forcefield]
    if m.dielec == "cdie": tail += " --cdie"
    if m.epsilon != 15: tail += " --epsilon %s" % (str(m.epsilon))    
    if g.calc_potentials == False: tail += " -calc-potentials=0"
    ret += "$ATTRACTDIR/make-grid%s %s %s %s %s %s %s\n\n" % \
     (tomp, f, ffpar, g.plateau_distance, g.neighbour_distance, gridfile, tail)
  ret += """
echo '**************************************************************'
echo 'Docking'
echo '**************************************************************'
"""
  itparams0 = ""
  if m.atomdensitygrids is not None:
    for g in m.atomdensitygrids:
      itparams0 += " --atomdensitygrid %s %s %s" % (g.voxelsize, g.dimension, g.forceconstant)
  ordinals = ["1st", "2nd", "3rd",] + ["%dth" % n for n in range(4,51)]
  iterations = []
  outp = ""
  if m.nr_iterations is None: m.nr_iterations = len(m.iterations)
  for n in range(m.nr_iterations):  
    if m.iterations is None or len(m.iterations) <= n:
      iterations.append([1500, 100, False, False])
    else:
      it = m.iterations[n]
      newit = [it.rcut, it.vmax, it.traj, it.mc]
      if it.mc:
        mcpar = (it.mctemp, it.mcscalerot, it.mcscalecenter, it.mcscalemode, it.mcensprob)
        newit.append(mcpar)
      iterations.append(newit)
  for i,it in enumerate(iterations):
    ret += """
echo '**************************************************************'
echo '%s minimization'
echo '**************************************************************'
""" % ordinals[i]
    itparams = itparams0
    rcut, vmax, traj, mc = it[:4]
    if rcut != 1500 and len(grid_used) == 0: itparams += " --rcut %s" % str(rcut)
    if vmax != 100: 
      if mc:
        itparams += " --mcmax %s" % str(vmax)
      else:
        itparams += " --vmax %s" % str(vmax)
    if traj: itparams += " --traj"
    if mc: 
      itparams += " --mc"
      mctemp, mcscalerot, mcscalecenter, mcscalemode, mscensprob = it[4]
      if mctemp != 3.5: itparams += " --mctemp %s" % str(mctemp)
      if mcscalerot != 0.05: itparams += " --mcscalerot %s" % str(mcscalerot)
      if mcscalecenter != 0.1: itparams += " --mcscalecenter %s" % str(mcscalecenter)
      if mcscalemode != 0.1: itparams += " --mcscalemode %s" % str(mcscalemode)
      if mcensprob != 0.1: itparams += " --mcensprob %s" % str(mcensprob)
    if m.np > 1:
      attract = "python $ATTRACTDIR/../protocols/attract.py"
      tail = "$parals --output"  
    else:
      attract = "$ATTRACTDIR/attract"
      tail = ">"  
    if i == len(iterations) - 1:
      outp = "out_$name.dat"
    else:  
      outp = "stage%d_$name.dat" % (i+1)  
    gridpar = ""
    if len(gridparams): gridpar = " $gridparams" 
    ret += "%s %s $params%s%s %s %s\n" % (attract, inp, gridpar, itparams, tail, outp)
    inp = outp       
  ret += "\n"  
  result = outp  
        
  if m.rescoring:
    ret += """
echo '**************************************************************'
echo 'Final rescoring'
echo '**************************************************************'
"""
    if m.np > 1:
      attract = "python $ATTRACTDIR/../protocols/attract.py"
      tail = "$parals --output"  
    else:
      attract = "$ATTRACTDIR/attract"
      tail = ">"  
    rcutsc = "--rcut %s" % str(m.rcut_rescoring)
    ret += "%s %s $scoreparams %s %s out_$name.score\n" \
     % (attract, result, rcutsc, tail)
    ret += """     
echo '**************************************************************'
echo 'Merge the scores with the structures'
echo '**************************************************************'
"""
    ret += "python $ATTRACTTOOLS/fill-energies.py %s out_$name.score > out_$name-scored.dat\n" % (result)
    ret += "\n"  
    result = "out_$name-scored.dat"

  if m.sort:
    ret += """
echo '**************************************************************'
echo 'Sort structures'
echo '**************************************************************'
"""
    ret += "python $ATTRACTTOOLS/sort.py %s > out_$name-sorted.dat\n" % result
    ret += "\n"  
    result = "out_$name-sorted.dat"

  if m.iattract:
    flexpar_iattract = ""
  if m.deredundant:    
    flexpar1 = ""
    if ens_any:
      flexpar1 += " --ens"
      if m.iattract: 
        flexpar_iattract += " --ens"
      for p in m.partners:
        ensemble_size = p.ensemble_size
        flexpar1 += " %d" % ensemble_size
        if m.iattract: 
          flexpar_iattract += " %d" % ensemble_size        
      if m.deredundant_ignorens: flexpar1 += " --ignorens"
    if modes_any:
      flexpar1 += " --modes"
      for p in m.partners:
        nr_modes = p.nr_modes
        flexpar1 += " %d" % nr_modes
    
    if m.fix_receptor == False: 
      ret += """
echo '**************************************************************'
echo 'Fix all structures into the receptor's coordinate frame
echo '**************************************************************'
"""     
      fixresult = result + "-fixre"
      ret += "$ATTRACTDIR/fix_receptor %s %d%s > %s\n" % (result, len(m.partners), flexpar1, fixresult)
      result = fixresult
    
    outp = os.path.splitext(result)[0] +"-dr.dat"
    ret += """
echo '**************************************************************'
echo 'Remove redundant structures'
echo '**************************************************************'
"""     
    ret += "$ATTRACTDIR/deredundant %s %d%s > %s\n" % (result, len(m.partners), flexpar1, outp)
    ret += "\n"  
    result = outp

  if m.iattract is not None:
    ret += """
echo '**************************************************************'
echo 'iATTRACT refinement'
echo '**************************************************************'
"""           
    assert p.moleculetype == "Protein" #for now, if you use iATTRACT, it must be a protein
    if m.calc_lrmsd or m.calc_irmsd or m.calc_fnat:
      for pnr, p in enumerate(m.partners):
        if p.modes_file or p.generate_modes:
          assert p.deflex == True, pnr+1 #for now, if you use iATTRACT, all modes must be removed in the analysis
          assert m.demode == True, pnr+1 #for now, if you use iATTRACT, all modes must be removed for collect      
    iattract_input = "out_$name-pre-iattract.dat"
    iattract_output = "out_$name-iattract.dat"
    iattract_output_demode = "out_$name-iattract-demode.dat"
    ret += "$ATTRACTTOOLS/top %s %d > %s\n" % (result, m.iattract.nstruc, iattract_input)
    iattract_params = ""
    iattract_params += " --infinite" #for now, use attract-infinite with iATTRACT
    iattract_params += " " + iattract_input
    iattract_params += " " + parameterfiledict["OPLSX"]
    iattract_params += " --cdie --epsilon 10 --fix-receptor"
    for f in iattract_filenames:
      iattract_params += " " + f
    noflex = [str(pnr+1) for pnr, p in enumerate(m.partners) if p.iattract_noflex]
    if len(noflex):
      iattract_params += " --noflex " + " ".join(noflex)
    iattract_params += " --icut %s" % m.iattract.icut
    iattract_params += " --np %s" % m.np
    if ens_any:
      for pnr,p in enumerate(m.partners):
        f = iattract_ensemble_lists[pnr]
        if f is not None:
          iattract_params += " --ens %d %s" % (pnr+1, f)
    if modes_any:
      iattract_params += " --modes %s" % iattract_modesfile    
    topfile = topologyfiledict["Protein", "OPLSX"] #TODO
    if m.runname == "attract":
      iname = "i$name"
    else:
      iname = "iattract-$name"
    ret += "python $ATTRACTDIR/../protocols/iattract.py %s --top %s --name %s --output %s\n" % (iattract_params, topfile, iname, iattract_output)
    result = iattract_output
    if m.demode:
      ret += "python $ATTRACTTOOLS/demode.py %s > %s\n" % (iattract_output, iattract_output_demode)    
      result = iattract_output_demode
    if m.fix_receptor or m.deredundant or m.calc_lrmsd:
      iattract_output_fixre = result + "-fixre"
      ret += "$ATTRACTDIR/fix_receptor %s %d%s > %s\n" % (result, len(m.partners), flexpar_iattract, iattract_output_fixre)        
      if m.fix_receptor or m.deredundant:
         result = iattract_output_fixre
  ret += """
echo '**************************************************************'
echo 'Soft-link the final results'
echo '**************************************************************'
"""       
  ret += "ln -s %s result.dat\n" % result

  result0 = result
  if m.calc_lrmsd or m.calc_irmsd or m.calc_fnat:    
    flexpar2 = ""    
    if m.calc_lrmsd or m.calc_irmsd or m.calc_fnat:
      deflex_any = any((p.deflex for p in m.partners))
      if modes_any and not m.demode and not deflex_any: 
        if unreduced_modes_any:
          flexpar2 = " --modes hm-all-unreduced.dat"
        else:
          flexpar2 = " --modes hm-all.dat"
      for pnr,p in enumerate(m.partners):
        if p.deflex == False and ensemble_lists[pnr] is not None:
          flexpar2 += " --ens %d %s" % (pnr+1,ensemble_lists[pnr])
  
    if deflex_any:
      deflex_header = """
echo '**************************************************************'
echo 'Remove flexibility for RMSD calculations'
echo '**************************************************************'
tmpf=`mktemp`
tmpf2=`mktemp`

""" 
      outp, outp2 = "$tmpf", "$tmpf2"
      any_modes = False
      any_ens = False
      for pnr,p in enumerate(m.partners):
        if p.deflex:
          if p.nr_modes:           
            any_modes = True
          elif p.ensemble_size:
            any_ens = True
      if any_modes and not m.iattract:           
        ret += deflex_header
        ret += "python $ATTRACTTOOLS/demode.py %s > %s\n" % \
          (result, outp)
        result = outp
        outp, outp2 = outp2, outp          
      if any_ens:
        if not (any_modes and not m.iattract):
          ret += deflex_header
        ret += "python $ATTRACTTOOLS/de-ensemblize.py %s > %s\n" % \
          (result,outp)
        result = outp
        outp, outp2 = outp2, outp

    any_bb = False
    rmsd_filenames = []
    for pnr in range(len(m.partners)):      
      p = m.partners[pnr]
      filename = filenames[pnr]
      if m.calc_lrmsd and p.rmsd_bb:
        if pnr > 0:
          if not any_bb:  
            any_bb = True
            ret += """
echo '**************************************************************'
echo 'Select backbone atoms'
echo '**************************************************************'
"""           
          filename2 = os.path.splitext(filename)[0] + "-bb.pdb"
          ret += "$ATTRACTTOOLS/backbone %s > %s\n" % (filename, filename2)
          filename = filename2
      rmsd_filenames.append(filename)
    if m.calc_irmsd:
      irmsd_filenames = [None] * len(m.partners)
      irmsd_refenames = [None] * len(m.partners)
      for pnr in range(len(m.partners)):
        p = m.partners[pnr]
        filename = p.pdbfile.name
        irmsd_filenames[pnr] = filename
        if p.ensemble_size > 0:
          irmsd_filenames[pnr] = p.collect_pdb.name
        if p.rmsd_pdb is not None:
          filename = p.rmsd_pdb.name
        irmsd_refenames[pnr] = filename
    if m.calc_fnat:
      fnat_filenames = [None] * len(m.partners)
      fnat_refenames = [None] * len(m.partners)
      for pnr in range(len(m.partners)):
        p = m.partners[pnr]
        filename = p.pdbfile.name
        fnat_filenames[pnr] = filename
        if p.ensemble_size > 0:
          fnat_filenames[pnr] = p.collect_pdb.name
        if p.rmsd_pdb is not None:
          filename = p.rmsd_pdb.name
        fnat_refenames[pnr] = filename
    if m.calc_lrmsd:
      lrmsd_filenames = rmsd_filenames[1:]
      lrmsd_refenames = [None] * (len(m.partners)-1)
      for pnr in range(1,len(m.partners)):
        filename = filenames[pnr]
        p = m.partners[pnr]
        if p.rmsd_pdb is not None:
          filename = p.rmsd_pdb.name
          if p.rmsd_bb:
            filename2 = os.path.splitext(filename)[0] + "-bb.pdb"
            ret += "$ATTRACTTOOLS/backbone %s > %s\n" % (filename, filename2)
            filename = filename2                
        elif p.rmsd_bb:
          filename2 = os.path.splitext(filename)[0] + "-bb.pdb"
          filename = filename2          
        lrmsd_refenames[pnr-1] = filename
        
    if any_bb:  
      ret += "\n"
      bb_str = "backbone "
    else:
      bb_str = ""
  if m.calc_lrmsd:      
    ret += """
echo '**************************************************************'
echo 'calculate %sligand RMSD'
echo '**************************************************************'
""" % bb_str      
  
    if m.fix_receptor == False and (not m.deredundant or m.iattract): 
      fixresult = result + "-fixre"
      ret += "$ATTRACTDIR/fix_receptor %s %d%s > %s\n" % (result, len(m.partners), flexpar2, fixresult)
      result = fixresult
  
    lrmsd_allfilenames = []
    for f1, f2 in zip(lrmsd_filenames, lrmsd_refenames):
      lrmsd_allfilenames.append(f1)
      lrmsd_allfilenames.append(f2)
    lrmsd_allfilenames = " ".join(lrmsd_allfilenames)
    lrmsdresult = os.path.splitext(result0)[0] + ".lrmsd"
    ret += "$ATTRACTDIR/lrmsd %s %s%s > %s\n" % (result, lrmsd_allfilenames, flexpar2, lrmsdresult)
    ret += "ln -s %s result.lrmsd\n" % lrmsdresult
    ret += "\n"

  if m.calc_irmsd:      
    ret += """
echo '**************************************************************'
echo 'calculate %sinterface RMSD'
echo '**************************************************************'
""" % bb_str      
    
    irmsd_allfilenames = []
    for f1, f2 in zip(irmsd_filenames, irmsd_refenames):
      irmsd_allfilenames.append(f1)
      irmsd_allfilenames.append(f2)
    irmsd_allfilenames = " ".join(irmsd_allfilenames)
    irmsdresult = os.path.splitext(result0)[0] + ".irmsd"
    bbo = "" if any_bb else "--allatoms"
    ret += "python $ATTRACTDIR/irmsd.py %s %s%s %s > %s\n" % (result, irmsd_allfilenames, flexpar2, bbo, irmsdresult)
    ret += "ln -s %s result.irmsd\n" % irmsdresult
    ret += "\n"

  if m.calc_fnat:      
    ret += """
echo '**************************************************************'
echo 'calculate fraction of native contacts'
echo '**************************************************************'
"""    
    fnat_allfilenames = []
    for f1, f2 in zip(fnat_filenames, fnat_refenames):
      fnat_allfilenames.append(f1)
      fnat_allfilenames.append(f2)
    fnat_allfilenames = " ".join(fnat_allfilenames)
    fnatresult = os.path.splitext(result0)[0] + ".fnat"
    ret += "python $ATTRACTDIR/fnat.py %s 5 %s%s > %s\n" % (result, fnat_allfilenames, flexpar2, fnatresult)
    ret += "ln -s %s result.fnat\n" % fnatresult
    ret += "\n"

  if m.calc_lrmsd or m.calc_irmsd or m.calc_fnat:
    if deflex_any:
      ret += "rm -f $tmpf $tmpf2\n"
      result = result0
    if m.calc_lrmsd:
      if m.fix_receptor == False:
        ret += "rm -f %s\n" % fixresult
    
  if m.collect:
    collect_filenames = filenames
    nr = m.nr_collect
    for pnr in range(len(m.partners)):
      p = m.partners[pnr]
      if p.collect_pdb is not None:
        collect_filenames[pnr] = p.collect_pdb.name
    ret += """
echo '**************************************************************'
echo 'collect top %d structures:'
echo '**************************************************************'
""" % nr
    ret += "$ATTRACTTOOLS/top %s %d > out_$name-top%d.dat\n" % (result, nr, nr)
    collect_filenames = " ".join(collect_filenames)
    demodestr = ""
    flexpar_unreduced = ""
    if modes_any:      
      if not m.demode: 
        if unreduced_modes_any:
          flexpar_unreduced = " --modes hm-all-unreduced.dat"
        else:
          flexpar_unreduced = " --modes hm-all.dat"    
      elif not m.iattract:        
        demodestr = "-demode"
        ret += "python $ATTRACTTOOLS/demode.py out_$name-top%d.dat > out_$name-top%d.dat-demode\n" % (nr, nr)
    for pnr,p in enumerate(m.partners):
      if collect_ensemble_lists[pnr] is not None:
        flexpar_unreduced += " --ens %d %s" % (pnr+1,collect_ensemble_lists[pnr])    
      elif ensemble_lists[pnr] is not None:
        flexpar_unreduced += " --ens %d %s" % (pnr+1,ensemble_lists[pnr])    
    ret += "$ATTRACTDIR/collect out_$name-top%d.dat%s %s%s > out_$name-top%d.pdb\n" % \
     (nr, demodestr, collect_filenames, flexpar_unreduced, nr)
    ret += "ln -s out_$name-top%d.pdb result.pdb\n" % nr
    ret += "\n"
  if len(cleanupfiles):  
    ret += "\nrm -f " + " ".join(cleanupfiles) + "\n"
  if (m.footer is not None):
    ret += m.footer + "\n\n"
    
  ret += "fi ### move to disable parts of the protocol\n"
  ret = ret.replace("\n\n\n","\n\n")
  ret = ret.replace("\n\n\n","\n\n")
  ret = ret.replace("\r\n", "\n")
  return ret

