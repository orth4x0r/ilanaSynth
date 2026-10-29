#!/usr/bin/env python3
"""Fit the Piano exciter (PianoModelTuning.h) to a real grand, on band levels
relative to each note's own pitch (fixed third-octave bands sample a partial
comb at random and mislead).

usage: python3 tools/fit_piano_bands.py [seconds]

Needs the University of Iowa MIS grand piano, mf, every third semitone from C1
(Piano.mf.<Note>.aiff, note names Db Eb Gb Ab Bb; theremin.music.uiowa.edu,
sound files/MIS/Piano_Other/piano/) in $ILANA_PIANO_REF (default
/tmp/piano-ref2), and a built ilanaTableTest ($ILANA_TABLE_TEST).

Renders the Felt Hammer Board preset at those notes (ILANA_NOTE_* variables,
ILANA_PIANO2_TUNING for the constants being fitted) and scores it against each
recording: the energy in bands that are multiples of the note's own f0 (0.85-1.5,
1.5-2.5, 2.5-3.5, 3.5-4.5, 4.5-6.5, 6.5-9, 9-13, 13-20, 20-32, 32-50 x f0, to
12 kHz) as a share of the total, in three windows (0.05-0.35 s, 0.8-1.3 s,
2-2.5 s), rms over bands and windows. Half the notes (every other one) are fitted,
the other half only scored. Coordinate search: a random subset of the settings is
stepped up and down in parallel, the best move is kept, steps shrink when nothing
helps. Log and best set go to $ILANA_FIT_WORK (default /tmp/piano-fit).
(2026-10-28 run: 14.5 -> 7.0 dB over all notes, 7.3 on the unfitted half.)
"""
import concurrent.futures as cfu
import json
import os
import random
import subprocess
import sys
import time
import wave

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
EXE = os.environ.get("ILANA_TABLE_TEST", os.path.join(ROOT, "build", "ilanaTableTest_artefacts", "Release", "ilanaTableTest"))
REFDIR = os.environ.get("ILANA_PIANO_REF", "/tmp/piano-ref2")
WORK = os.environ.get("ILANA_FIT_WORK", "/tmp/piano-fit")
PRESET = "Felt Hammer Board"
os.makedirs(WORK, exist_ok=True)

def rd(f):
    w=wave.open(f);sw=w.getsampwidth();ch=w.getnchannels();sr=w.getframerate();d=w.readframes(w.getnframes())
    if sw==3:
        b=np.frombuffer(d,np.uint8).reshape(-1,3);v=(b[:,0].astype(np.int32)|b[:,1].astype(np.int32)<<8|b[:,2].astype(np.int32)<<16);v=np.where(v>=1<<23,v-(1<<24),v)/8388608.
    elif sw==2: v=np.frombuffer(d,'<i2')/32768.
    else: raise Exception(sw)
    v=v.reshape(-1,ch).mean(1);return v,sr

def aiff(f):
    d=open(f,'rb').read();i=12;ch=2;bits=16;sr=44100
    while i<len(d):
        c=d[i:i+4];n=int.from_bytes(d[i+4:i+8],'big')
        if c==b'COMM':
            ch=int.from_bytes(d[i+8:i+10],'big');bits=int.from_bytes(d[i+14:i+16],'big')
            e=int.from_bytes(d[i+16:i+18],'big');m=int.from_bytes(d[i+18:i+26],'big');sr=m*2.0**(e-16383-63)
        if c==b'SSND':
            raw=d[i+16:i+8+n]
        i+=8+n+(n&1)
    a=np.frombuffer(raw[:len(raw)//(bits//8)*(bits//8)],'>i2' if bits==16 else None)/32768.
    return a.reshape(-1,ch).mean(1),int(sr)

NAMES=['C','Db','D','Eb','E','F','Gb','G','Ab','A','Bb','B']
ALL=list(range(24,109,3))
def nm(m): return NAMES[m%12]+str(m//12-1)
REF2={}
def refs():
    for m in ALL:
        f=f''+REFDIR+'/Piano.mf.{nm(m)}.aiff'
        if os.path.exists(f) and os.path.getsize(f)>100000 and m not in REF2:
            REF2[m]=aiff(f)
    return REF2
def render(tag,tuning="",overrides="",notes=None):
    notes=notes or ALL
    folder=os.path.join(WORK,tag);os.makedirs(folder,exist_ok=True)
    env=dict(os.environ,ILANA_NOTE_DEBUG=PRESET,ILANA_NOTE_SET="keyboard",ILANA_NOTE_LIST=",".join(map(str,notes)),ILANA_NOTE_DYNAMICS="mf",ILANA_NOTE_SECONDS="4",ILANA_NOTE_FOLDER=folder,ILANA_PIANO2_TUNING=tuning,ILANA_PRESET_OVERRIDES=overrides)
    subprocess.run([EXE],env=env,cwd=ROOT,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True)
    return {n:rd(f'{folder}/ours.mf.n{n}.wav') for n in notes}
EDGES=[0.85,1.5,2.5,3.5,4.5,6.5,9,13,20,32,50]
def f0of(m): return 440*2**((m-69)/12)
def bands(x,sr,t0,t1,i0,f0):
    s=x[i0+int(t0*sr):i0+int(t1*sr)]
    if len(s)<int(.2*sr): return None
    X=np.abs(np.fft.rfft(s*np.hanning(len(s)),1<<17))**2;fr=np.fft.rfftfreq(1<<17,1/sr)
    out=[]
    for lo,hi in zip(EDGES[:-1],EDGES[1:]):
        l,h_=lo*f0,min(hi*f0,12000)
        out.append(X[(fr>=l)&(fr<h_)].sum()+1e-12 if l<12000 else np.nan)
    out=np.array(out);tot=np.nansum(out)
    return 10*np.log10(out/tot)
def onset(x,sr):
    e=np.convolve(np.abs(x),np.ones(int(sr*0.005))/int(sr*0.005),'same');return max(0,int(np.argmax(e>0.15*e.max()))-int(0.004*sr))
WIN=((0.05,0.35),(0.8,1.3),(2.0,2.5))
RB={}
def prep():
    refs()
    for m,(x,sr) in REF2.items():
        if m not in RB: RB[m]=[bands(x,sr,a,b,onset(x,sr),f0of(m)) for a,b in WIN]
def err(R,notes=None,detail=False):
    prep();det={}
    for m in (notes or list(R)):
        if m not in RB: continue
        x,sr=R[m];e=[]
        for k,(a,b) in enumerate(WIN):
            s=bands(x,sr,a,b,onset(x,sr),f0of(m));r=RB[m][k]
            if s is None or r is None: continue
            d=s-r;d=d[~np.isnan(d)];e.append(float(np.sqrt((d**2).mean())))
        if e: det[m]=round(float(np.mean(e)),1)
    mean=float(np.mean(list(det.values())))
    return (mean,det) if detail else mean

P=[('boardEq%d'%i,v,-15,15,1.5) for i,v in enumerate([9.0,2.15,-5.43,3.16,1.59,3.74,0.17,-3.23,-7.2,-9.0])]+[
   ('dip2',0,0,24,3),('dip3',0,0,24,3),('dip4',0,0,24,3),('dip5',15,0,24,3),('dipQ',1.2,0.5,3,0.2),
   ('exponentBass',2.05762,1.6,3.5,0.15),('exponentTreble',2.82263,2.0,4.0,0.2),
   ('strikeBass',0.12705,0.06,0.2,0.012),('strikeTreble',0.0929744,0.03,0.16,0.01),
   ('impedanceC4',1.33796,0.6,4.0,0.2),('impedanceBass',4.9012,2.0,20,0.8),
   ('aftersound',0.22463,0,1,0.06),('horizontalMix',0.666726,0.1,3,0.15),('horizontalCents',0.5098,-1.5,1.5,0.15),
   ('knock',0.158342,0,0.4,0.02),('knockFrequency',500,300,6000,'log'),
   ('massBass',11.8894,5,20,0.8),('feltBass',65,15,600,'log'),('hysteresis',0.87,0,2,0.15),
   ('massTreble',6.43,1.5,9,0.6),('feltTreble',20,10,1500,'log'),('speedHigh',3.56864,2,8,0.3),
   ('t60Upper',18.45,1,30,2.0),('promptRatio',0.1625,0.03,0.6,0.04),('t60Register',0.704095,0.2,1.2,0.07),
   ('t60RegisterTreble',0.784,0.1,1.0,0.07),('t60C4',15.775,4,40,2.0),('boardMix',2.95198,0.3,6,0.4),('boardTilt',0.569308,0,1.2,0.1)]

def ts(v):
    return ';'.join(f'{k}={v[k]:.5g}' for k in v)


def clamp(p, x):
    return min(max(x, p[2]), p[3])


if __name__ == "__main__":
    TRAIN = ALL[0::2]
    random.seed(1)
    seconds = float(sys.argv[1]) if len(sys.argv) > 1 else 3000
    cur = {p[0]: p[1] for p in P}
    step = {p[0]: (p[4] if p[4] != 'log' else 0.3) for p in P}
    best = err(render('k_cur', tuning=ts(cur), notes=TRAIN))
    print('start', best, flush=True)
    log = open(os.path.join(WORK, 'fit.log'), 'a')
    pool = cfu.ThreadPoolExecutor(3)
    it = 0
    t0 = time.time()
    while time.time() - t0 < seconds:
        it += 1
        sub = random.sample(P, 8)
        cands = []
        for p in sub:
            for sgn in (1, -1):
                v = dict(cur)
                s = step[p[0]]
                v[p[0]] = clamp(p, v[p[0]] * (1 + sgn * s) if p[4] == 'log' else v[p[0]] + sgn * s)
                if v[p[0]] != cur[p[0]]:
                    cands.append((p[0], sgn, v))
        futs = [pool.submit(lambda c=c, i=i: err(render(f'k_{i}', tuning=ts(c[2]), notes=TRAIN))) for i, c in enumerate(cands)]
        res = sorted(((f.result(), c) for f, c in zip(futs, cands)), key=lambda r: r[0])
        if res and res[0][0] < best - 0.02:
            best = res[0][0]
            cur = res[0][1][2]
        else:
            for p in sub:
                step[p[0]] *= 0.7
        print(it, round(best, 2), flush=True)
        log.write(json.dumps({'it': it, 'best': best, 'cur': cur}) + '\n')
        log.flush()
    json.dump(cur, open(os.path.join(WORK, 'fit_best.json'), 'w'))
    m, d = err(render('k_all', tuning=ts(cur)), detail=True)
    print('done', round(best, 2), 'all notes', round(m, 2), ts(cur))
