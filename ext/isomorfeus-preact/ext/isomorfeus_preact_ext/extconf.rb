require 'mkmf'

$CFLAGS << ' -O3 -Wall '

create_makefile('isomorfeus_preact_ext')
