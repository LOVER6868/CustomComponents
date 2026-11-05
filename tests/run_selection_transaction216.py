#!/usr/bin/env python3
"""Host regressions and full syntax; requires a C++20 compiler + Sprig/BNM headers.
python3 tests/run_selection_transaction216.py --sprig /path/Sprig-main --deps /path/deps
No Android linking or device execution is performed.
"""
from pathlib import Path
import argparse, subprocess, tempfile, json
p=argparse.ArgumentParser()
p.add_argument('--sprig',type=Path,required=True)
p.add_argument('--deps',type=Path,required=True)
p.add_argument('--compiler',default='g++')
p.add_argument('--report',type=Path)
a=p.parse_args();root=Path(__file__).resolve().parents[1];host=root/'tests/host'
flags=['-std=c++20','-O1','-ffunction-sections','-fdata-sections','-include',str(host/'host_compat.hpp')]
for folder in [root/'inc',host,a.sprig/'mod/include',a.deps/'BNM-Android/include',a.deps/'Dobby/include']:
 flags+=['-I',str(folder.resolve())]
results=[]
def run(name,cmd):
 r=subprocess.run(cmd,capture_output=True,text=True)
 results.append({'test':name,'passed':r.returncode==0,'output':(r.stdout+r.stderr)[-4000:] if r.returncode else r.stdout[-1200:]})
 if r.returncode:print('FAIL',name)
 else:print('PASS',name)
 return r.returncode==0
with tempfile.TemporaryDirectory() as d:
 cases=['SelectionTransaction216Regression','SelectionTransaction216RuntimeRegression',
 'SelectionTransaction219RuntimeRegression','SelectionResolution219Regression','SelectionTransaction220DatabaseRegression','SelectionTransaction221AutomaticRegression','SelectionTransaction222AutomaticRegression','PhysicalDeckSelection222Regression',
 'Patch211DualDomainRegression','Patch2InFlightSelection29Regression','Patch2ExactRouting25Regression',
 'Patch2Routing26ValidationRegression','Patch2SnapshotHistory1730Regression','Patch2SnapshotHistory1731Regression',
 'Patch2ExactSnapshot1733Regression','Patch2ExecutionScope22Regression',
 'Pick3DatabaseTargetContextRegression','Pick3VerifiedBindingRegression']
 for name in cases:
  exe=str(Path(d)/name)
  if run(name+' compile',[a.compiler,*flags,str(root/'tests'/f'{name}.cpp'),str(host/'log_stub.cpp'),'-Wl,--gc-sections','-o',exe]):
   run(name,[exe])
 for name,defines in [('routing-off',['-DCUSTOMCOMPONENTS_PF3_TRANSACTION_ROUTING=0'])]:
  exe=str(Path(d)/name)
  if run(name+' compile',[a.compiler,*flags,*defines,str(root/'tests/SelectionTransaction216Regression.cpp'),'-o',exe]):run(name,[exe])
 for name,defines in [('full-syntax',[]),('rollback-syntax',['-DCUSTOMCOMPONENTS_PF3_TRANSACTION_PIPELINE=0','-DCUSTOMCOMPONENTS_PF3_EXACT_ROUTING=0','-DCUSTOMCOMPONENTS_PF3_ENTITY_LINK_ROUTING=0'])]:
  run(name,[a.compiler,*flags,*defines,'-DBLOOM_MODULE_NAME="CustomComponents"','-DBLOOM_MODULE_TYPE=Bloom::Type::PnP','-Wno-invalid-offset','-fsyntax-only',str(root/'main.cpp')])
 for name in ['Patch210DispatchProofOnceSourceRegression.py','Patch2InFlightSelection29SourceRegression.py','Patch262QueryLoggingSafetyRegression.py','Patch28DeferredSafetyRegression.py']:
  run(name,['python3',str(root/'tests'/name)])
if a.report:a.report.write_text(json.dumps(results,indent=2)+'\n')
raise SystemExit(0 if all(r['passed'] for r in results) else 1)
