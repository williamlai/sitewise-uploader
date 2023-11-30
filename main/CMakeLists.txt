idf_component_register(SRCS 
    "main.c"
    "sitewise.c"
    "sitewise.h"
    "sitewise_uploader.c"
    "sitewise_uploader.h"
    "dht.c"
    "dht.h"
    "aws_sig_v4_signing.c"
    "aws_sig_v4_signing.h"
    INCLUDE_DIRS "."
)
set_source_files_properties(aws_sig_v4_signing.c PROPERTIES COMPILE_FLAGS "-Wno-restrict")
