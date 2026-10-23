/* Included at the end of gnuplot's plot.c (see patches/0003-embed-hook.patch)
 * so it can reach plot.c's statics. Gives the lmplot host an in-process,
 * non-interactive gnuplot: init once, then run command strings, with errors
 * returned instead of exiting. */
static int lmplot_ready = 0;

int lmplot_gp_init(void)
{
    if (lmplot_ready)
	return 0;
    setbuf(stderr, (char *) NULL);
    gpoutfile = stdout;
    (void) add_udv_by_name("GNUTERM");
    (void) add_udv_by_name("I");
    (void) add_udv_by_name("NaN");
    init_constants();
    udv_user_head = &(udv_NaN->next_udv);
    init_memory();
    interactive = FALSE;
    skip_gnuplotrc = TRUE;
    if (!SETJMP(command_line_env, 1)) {
	interrupt_setup();
	get_user_env();
	init_loadpath();
	init_locale();
	memset(&sm_palette, 0, sizeof(sm_palette));
	init_fit();
	init_encoding();
	init_gadgets();
	init_terminal();
	push_terminal(0);
	gp_atexit(term_reset);
	show_version(NULL);
	update_gpval_variables(3);
	init_session();
	lmplot_ready = 1;
	return 0;
    }
    return -1;
}

/* Run one gnuplot command line (or ';'-separated lines). 0 = ok, -1 = error;
 * after an error the text is in GPVAL_ERRMSG. */
int lmplot_gp_do(const char *cmd)
{
    if (!lmplot_ready)
	return -1;
    if (!SETJMP(command_line_env, 1)) {
	do_string(cmd);
	return 0;
    }
    clause_reset_after_error();
    lf_reset_after_error();
    inside_plot_command = FALSE;
    return -1;
}

void lmplot_gp_shutdown(void)
{
    if (lmplot_ready) {
	gp_exit_cleanup();
	lmplot_ready = 0;
    }
}

/* Evaluate `expr` and fetch the result. kind: 0 error, 1 number, 2 string,
 * 3 complex with a non-zero imaginary part, 4 another type (array, datablock,
 * function block, voxel grid). The string is malloc'ed (free it); numbers
 * come back in *num. Types are handled here rather than with real(), whose
 * int_error on non-scalars would longjmp to a command_line_env whose frame
 * (lmplot_gp_do) has already returned. */
int lmplot_gp_eval(const char *expr, double *num, char **str)
{
    struct udvt_entry *u;
    size_t n = strlen(expr) + 32;
    char *cmd = (char *) malloc(n);
    int kind = 0;
    if (!cmd)
	return 0;
    snprintf(cmd, n, "lmplot_result__ = (%s)", expr);
    if (lmplot_gp_do(cmd) == 0 && (u = get_udv_by_name("lmplot_result__")) != NULL) {
	switch (u->udv_value.type) {
	case STRING:
	    if (u->udv_value.v.string_val) {
		*str = strdup(u->udv_value.v.string_val);
		kind = *str ? 2 : 0;
	    }
	    break;
	case INTGR:
	    *num = (double) u->udv_value.v.int_val;
	    kind = 1;
	    break;
	case CMPLX:
	    if (u->udv_value.v.cmplx_val.imag != 0.0) {
		kind = 3;
	    } else {
		*num = u->udv_value.v.cmplx_val.real;
		kind = 1;
	    }
	    break;
	case NOTDEFINED:
	    break;
	default:
	    kind = 4;
	    break;
	}
    }
    free(cmd);
    return kind;
}

/* After a command: flush the output file and report write errors (disk full,
 * /dev/full, closed pipe), which gnuplot's terminals mostly ignore. 1 = ok. */
int lmplot_gp_output_ok(void)
{
    if (gpoutfile && gpoutfile != stdout && gpoutfile != stderr)
	return fflush(gpoutfile) == 0 && !ferror(gpoutfile);
    return 1;
}

/* Text of the last gnuplot error (GPVAL_ERRMSG), or "". */
const char *lmplot_gp_errmsg(void)
{
    struct udvt_entry *u = get_udv_by_name("GPVAL_ERRMSG");
    if (u && u->udv_value.type == STRING && u->udv_value.v.string_val)
	return u->udv_value.v.string_val;
    return "";
}

/* --dry-run (see plot.h): files gnuplot would write go to the null device instead. */
int lmplot_dry_run = 0;

FILE *lmplot_dry_fopen(const char *name, const char *mode)
{
    if (!lmplot_dry_run)
	return fopen(name, mode);
    fprintf(stderr, "[dry-run] would write %s\n", name);
#ifdef _WIN32
    {
	/* fopen("NUL") is not reliable through every Windows C runtime/wrapper. */
	FILE *f = fopen("NUL", mode);
	if (!f)
	    f = fopen("\\\\.\\NUL", mode);
	if (!f)
	    f = tmpfile();
	return f;
    }
#else
    return fopen("/dev/null", mode);
#endif
}
