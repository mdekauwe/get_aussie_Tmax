//
//
// PROGRAM:
//              get_lt_aussie_tmax
//
// DESCRIPTION:
//              Using the eMAST Tmax data estimate the long-term N-days Tmax
//              for each pixel across the continent. Where N might be 3, 4 or
//              5 days, not decided yet.
//
// AUTHOR:      Martin De Kauwe
//
// EMAIL:       mdekauwe@gmail.com
//
// DATE:        10th November, 2016
//

#include "get_lt_aussie_tmax.h"

int main(int argc, char **argv) {

    int   jj, rr, cc, status, nc_id, var_id, yr, ndays, days_in_mth;
    int   mth_id, day, dd, nmonths = 3, nday_idx, yr_idx;
    int   x_dimid, y_dimid;
    int   dimids[NDIMS];
    long  offset;
    char  imth[3];
    char  iday[3];
    char  infname[STRING_LENGTH];
    char  ofname1[STRING_LENGTH];
    char  ofname2[STRING_LENGTH];

    // Need to be declared like this otherwise the netcdf read will attempt to
    // use the heap rather than the stack to allocate memory and run out.
    // I'm sure there is a way to get netcdf to read into a 1D array but I
    // don't know how to do that and can't find a quick example
    // NB. I'm declaring 1 extra spot for leap years, so we will need to make
    // sure when we read from this array we are checking leap years.
    static float data_in[MAX_DAYS][NLAT][NLON];
    static float nc_data_out1[NLAT][NLON];
    static float nc_data_out2[NLAT][NLON];
    float        max_5day_sum, sum, value;
    float       *data_out = NULL;
    float       *data_out2 = NULL;
    int         *cnt_all_yrs = NULL;

    control *c;
    c = (control *)malloc(sizeof (control));
    if (c == NULL) {
        fprintf(stderr, "control structure: Not allocated enough memory!\n");
        exit(EXIT_FAILURE);
    }

    if ((data_out = (float *)calloc(NLAT*NLON, sizeof(float))) == NULL) {
        fprintf(stderr, "Error allocating space for data_out array\n");
        exit(EXIT_FAILURE);
    }

    if ((data_out2 = (float *)calloc(NLAT*NLON, sizeof(float))) == NULL) {
        fprintf(stderr, "Error allocating space for data_out2 array\n");
        exit(EXIT_FAILURE);
    }

    if ((cnt_all_yrs = (int *)calloc(NLAT*NLON, sizeof(int))) == NULL) {
        fprintf(stderr, "Error allocating space for cnt_all_yrs array\n");
        exit(EXIT_FAILURE);
    }

    // Initial values, these can be changed on the cmd line
    strcpy(c->fdir, "/Users/mdekauwe/Downloads/emast_data");
    strcpy(c->var_name, "air_temperature");
    c->window = 5;
    c->start_yr = 1970;
    c->end_yr = 1971;

    clparser(argc, argv, c);


    yr_idx = 0;
    for (yr = c->start_yr; yr < c->end_yr; yr++) {
        printf("%d %d\n", yr, yr_idx);

        ndays = 0;
        nday_idx = 0;
        for(mth_id = 0; mth_id < nmonths; mth_id++) {

            // Dec, Jan Feb?
            if (mth_id == 0) {
                ndays += 31;
                days_in_mth = 31;
            } else if (mth_id == 1) {
                ndays += 31;
                days_in_mth = 31;
            } else if (mth_id == 2) {
                if (is_leap_year(yr+1)) {
                    ndays += 29;
                    days_in_mth = 29;
                } else {
                    ndays += 28;
                    days_in_mth = 28;
                }
            }

            for (day = 1; day <= days_in_mth; day++) {

                if (day < 10) {
	                sprintf(iday, "0%d", day);
	            } else {
	                sprintf(iday, "%d", day);
                }

                if (mth_id == 0) {
                    sprintf(imth, "12");
                } else if (mth_id == 1) {
                    sprintf(imth, "01");
                } else if (mth_id == 2) {
                    sprintf(imth, "02");
                }

                if (mth_id == 0) {
                    sprintf(infname,
                            "%s/eMAST_ANUClimate_day_tmax_v1m0_%d%s%s.nc",
                            c->fdir, yr, imth, iday);
                } else {
                    sprintf(infname,
                            "%s/eMAST_ANUClimate_day_tmax_v1m0_%d%s%s.nc",
                            c->fdir, yr+1, imth, iday);
                }

                // For now just read the same file again and again!
                //sprintf(infname,
                //        "%s/eMAST_ANUClimate_day_tmax_v1m0_20000101.nc",
                //        c->fdir);
                //printf("%s\n", infname);
                read_nc_file_into_array(c, infname, nday_idx, data_in);

                nday_idx++;

            }  // Day in month loop
        } // mth loop

        // Calculate the maximum n-day Tmax sum across this years Australian
        // summer for every pixel
        for (rr = 0; rr < NLAT; rr++) {
            //printf("%d : %d    %d\n", rr, NLAT, ndays);
            for (cc = 0; cc < NLON; cc++) {
                offset = rr * NLON + cc;
                max_5day_sum = -9999.9;
                for (dd = 0; dd < ndays - c->window; dd+=c->window) {
                    sum = 0.0;
                    for (jj = dd; jj < dd+c->window; jj++) {
                        value = data_in[jj][rr][cc];
                        // ignore masked values
                        if (value > -9000.0) {
                            sum += data_in[jj][rr][cc];
                        }
                    }
                    if (sum > max_5day_sum) {
                        data_out[offset] = sum;
                    }
                } // end day loop

                // Save running sum over all years so we can take the average
                // later to calculate the max accross all years
                if (data_out[offset] > 0.0) {
                    data_out2[offset] += data_out[offset];
                    cnt_all_yrs[offset]++;
                }

            } // end column loop
        } // end row loop

        yr_idx++;

    } // yr loop

    // Figure out maximum for each pixel across all years
    yr_idx = 0;
    for (yr = c->start_yr; yr < c->end_yr; yr++) {
        for (rr = 0; rr < NLAT; rr++) {
            for (cc = 0; cc < NLON; cc++) {
                offset = rr * NLON + cc;
                if (data_out2[offset] > 0.0) {
                    data_out2[offset] /= (float)cnt_all_yrs[offset];
                }
            }
        }
        yr_idx++;
    }

    // Write data to two netcdf files.

    // Copy data into netcdf structured array. I'm sure i don't need to do this
    // but I'm not sure how to pass 1D array to netcdf write! Pain for now
    for (rr = 0; rr < NLAT; rr++) {
        for (cc = 0; cc < NLON; cc++) {
            offset = rr * NLON + cc;
            nc_data_out1[rr][cc] = data_out[offset];
            nc_data_out2[rr][cc] = data_out2[offset];
        }
    }

    sprintf(ofname1, "5_day_Tmax_sum.nc");
    write_nc_file(ofname1, nc_data_out1);

    sprintf(ofname2, "5_day_Tmax_avg.nc");
    write_nc_file(ofname2, nc_data_out2);

    free(data_out);
    free(data_out2);
    free(cnt_all_yrs);
    free(c);

    return(EXIT_SUCCESS);

}

int is_leap_year(int year) {

    int leap = FALSE;
    if ((year % 4 == 0) && (year % 100 != 0) || (year % 400 == 0)) {
        leap = TRUE;
    }

    return (leap);

}

void clparser(int argc, char **argv, control *c) {
    int i;

    for (i = 1; i < argc; i++) {
        if (*argv[i] == '-') {
            if (!strncasecmp(argv[i], "-fd", 3)) {
                strcpy(c->fdir, argv[++i]);
            } else if (!strncasecmp(argv[i], "-vn", 3)) {
                strcpy(c->var_name, argv[++i]);
            } else if (!strncasecmp(argv[i], "-w", 2)) {
                c->window = atoi(argv[++i]);
            } else if (!strncasecmp(argv[i], "-sy", 3)) {
                c->start_yr = atoi(argv[++i]);
            } else if (!strncasecmp(argv[i], "-ey", 3)) {
                c->end_yr = atoi(argv[++i]);
            } else {
                fprintf(stderr,"%s: unknown argument on command line: %s\n",
                        argv[0], argv[i]);
                exit(EXIT_FAILURE);
            }
        }
    }
    return;
}

void read_nc_file_into_array(control *c, char *infname, int day_idx,
                 float nc_in[MAX_DAYS][NLAT][NLON]) {

    int  status, nc_id, var_id;

    if ((status = nc_open(infname, NC_NOWRITE, &nc_id))) {
        ERR(status);
    }

    if ((status = nc_inq_varid(nc_id, c->var_name, &var_id))) {
        ERR(status);
    }

    if ((status = nc_get_var_float(nc_id, var_id,
                                   &nc_in[day_idx][0][0]))) {
        ERR(status);
    }

    if ((status = nc_close(nc_id))) {
        ERR(status);
    }

    return;
}

void write_nc_file(char *ofname, float out[NLAT][NLON]) {

    int  status, nc_id, var_id, nday_idx, yr_idx;
    int  x_dimid, y_dimid;
    int  dimids[NDIMS];

    if ((status = nc_create(ofname, NC_CLOBBER, &nc_id))) {
        ERR(status);
    }

    if ((status = nc_def_dim(nc_id, "x", NLON, &x_dimid))) {
        ERR(status);
    }

    if ((status = nc_def_dim(nc_id, "y", NLAT, &y_dimid))) {
        ERR(status);
    }

    dimids[0] = y_dimid;
    dimids[1] = x_dimid;

    if ((status = nc_def_var(nc_id, "Tmax", NC_FLOAT, NDIMS,
                             dimids, &var_id))) {
        ERR(status);
    }

    if ((status = nc_enddef(nc_id))) {
        ERR(status);
    }

    if ((status = nc_put_var_float(nc_id, var_id, &out[0][0]))) {
        ERR(status);
    }

    if ((status = nc_close(nc_id))) {
        ERR(status);
    }

    return;
}
