[general]
fps=60
globalOffset=-30
; Ordinary match window. A unique, well-observed past note may use bounded late recovery.
maxWindow=150
mode=4
; Template set: cap{mode}_{blue|white}_{note}.png. Restart after changing.
; Use note=default for legacy unsuffixed cap{mode}_{color}.png files.
note=bluearchive
; All Y values below are pixels local to the captured ROI, increasing downward.
judgeY=750
noteThreshold=0.75
; Last corrected note-anchor Y to detect (not a timing offset).
maxDetectY=690
; Keep detections at least this far above judgeY to avoid judgment effects.
detectionMarginY=60
; Add to raw template-top Y. Positive moves the note down; 0 preserves old normal reference.
normalNoteAnchorY=0
; Add to side edge Y. -10 preserves the previous side-note reference.
sideNoteAnchorY=-10
; Avoid MAX judgment text/effects around Y=600 for wide side blocks.
sideMaxDetectY=580
maxMarkers=30
avgSamples=60
triggerColor=default
sideColor=default
showAVG=true
showEarlyLate=true
; Analysis panel + per-input observation CSVs. Restart after changing.
; true: per-key debug panel + detailed ruler labels/logs; false: centered EARLY/LATE only.
debugmode=false
; Legacy fallback when debugmode is absent. Explicit debugmode always wins.
showLaneDebug=true
; Fixed thick lane arrows: EARLY up / LATE down, 500ms fade. Independent of debugmode.
laneArrows=true
; Show arrows only when absolute timing error strictly exceeds this many milliseconds.
; 0: all nonzero errors; example 41.666: only errors beyond +/-41.666ms.
laneArrowThresholdMs=41.666
; Arrow center Y in the game client at reference 1920x1080 (not ruler/OBS coordinates).
laneArrowY=450
; Separate panel, outside the default gear capture (X=720..1199).
laneDebugX=1301
laneDebugY=0
perfectRangeColor=0,255,120
; Game-window-only capture excludes this overlay even when it covers the gear.
; OBS: capture Sepan Overlay separately as a Window Capture source.
overlayX=809
overlayY=199

[4LANE]
1=s
2=d
3=l
4=;
L=Space
R=RAlt
A=LShift
B=RShift

[5LANE]
1=a
2=s
3a=d
3b=l
4=;
5='
L=Space
R=RAlt
A=LShift
B=RShift

[6LANE]
1=a
2=s
3=d
4=l
5=;
6='
L=Space
R=RAlt
A=LShift
B=RShift
