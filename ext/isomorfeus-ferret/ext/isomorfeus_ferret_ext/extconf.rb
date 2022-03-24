require 'mkmf'

$CFLAGS << ' -O2 -Wall -Wno-sizeof-pointer-div'

create_makefile('isomorfeus_ferret_ext')
