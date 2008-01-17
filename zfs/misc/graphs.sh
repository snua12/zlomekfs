#!/bin/bash

ls -1 *.out | while read name; do
	echo $name
	./graphMaker ${name} ${name}.png ${name}.summary 2>${name}.err
done
