/* Support for GEMDOS extended vectors */

static void *old_term, *old_critic;
typedef void (* void_fun_pointer)(void);

static void new_term(void)
{
	void (*terminate)(void) = old_term;

	(void)Setexc(0x101, (void_fun_pointer)old_critic);
	(void)Setexc(0x102, (void_fun_pointer)old_term);
	terminate ();
}

static long /*cdecl*/ new_critic(int code)
{
	return code;
}

void DODisableCEH(void)
{
	if (getenv ("LEAVE_CEH_ALONE")) return;

	old_term = Setexc (0x0102, new_term);
	old_critic = Setexc (0x0101, (void_fun_pointer)new_critic);
}
