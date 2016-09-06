#ifndef _MS_CPR_H
#define _MS_CPR_H

#include "fields.h"

int decode_cpr_local(const struct ms_CPR_t*,
                     double lat_s, double lon_s,
                     double *, double*,
                     bool i);
int decode_cpr_global(const struct ms_CPR_t*,
                      const struct ms_CPR_t*,
                      double *, double*,
                      bool i);

#endif
