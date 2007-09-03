

echo '//
//	ipc.i	WJ103
//

%module pysyplog
%{
' > $1



find ../syplog -iname '*.h' | while read file; do echo '#include "'$file'"'; done >> $1

echo '
%}

// Produce constants and helper functions for structures and unions
' >>$1

find ../syplog -iname '*.h' | while read file; do echo '%include "'$file'"'; done >> $1

echo '

%inline
%{
%}

// EOB' >> $1