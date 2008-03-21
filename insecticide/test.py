#!/usr/bin/env python

""" Wrapper for nose usage under insecticide 
    (simple call this script instead of nosetests) 
"""

from insecticide.util import noseWrapper

noseWrapper(project = 'insecticide')
