"""Preserve numeric deployment settings without executing installed robot code."""
from __future__ import print_function
import ast
import math
import os
import re

KEYS=('BASE_ORTHOGONAL_SECURITY_M','BASE_TANGENTIAL_SECURITY_M','BASE_TEST_CAP')

def preserve(installed,candidate):
    result=candidate
    values={}
    for key in KEYS:
        match=re.search(r'^'+key+r'\s*=\s*([^\n]+)',installed,re.M)
        if not match:continue
        try:value=ast.literal_eval(match.group(1).split('#',1)[0].strip())
        except Exception:raise ValueError('Cannot preserve non-literal setting '+key)
        if key=='BASE_TEST_CAP' and value is None:pass
        elif isinstance(value,bool) or not isinstance(value,(int,float)) or math.isnan(float(value)) or math.isinf(float(value)) or value<=0:
            raise ValueError('Invalid installed setting '+key)
        elif key=='BASE_TEST_CAP' and value>.1:raise ValueError('Invalid installed test cap')
        values[key]=value
        replacement=key+' = '+repr(value)
        result,count=re.subn(r'^'+key+r'\s*=\s*[^\n]+',lambda unused:replacement,result,count=1,flags=re.M)
        if count!=1:raise ValueError('Missing bundled setting '+key)
    # Validate the merged pair even when only one setting existed previously.
    margins=[]
    for key in KEYS[:2]:
        match=re.search(r'^'+key+r'\s*=\s*([^\n]+)',result,re.M)
        if not match:raise ValueError('Missing bundled setting '+key)
        value=ast.literal_eval(match.group(1).split('#',1)[0].strip())
        if isinstance(value,bool) or not isinstance(value,(int,float)) or math.isnan(float(value)) or math.isinf(float(value)) or value<=0:
            raise ValueError('Invalid merged setting '+key)
        margins.append(value)
    if margins[0]<margins[1]:raise ValueError('Invalid installed base margin relationship')
    return result

if __name__=='__main__':
    import sys
    installed,candidate=sys.argv[1:3]
    if os.path.isfile(installed):
        with open(installed) as f:old=f.read()
        with open(candidate) as f:new=f.read()
        updated=preserve(old,new)
        with open(candidate,'w') as f:f.write(updated)
