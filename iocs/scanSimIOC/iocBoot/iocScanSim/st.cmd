#!/bin/bash

# Export library path in case building against shared libraries
export LD_LIBRARY_PATH=../../../../lib/linux-x86_64:$LD_LIBRARY_PATH

../../bin/linux-x86_64/scanSimApp st_base.cmd
