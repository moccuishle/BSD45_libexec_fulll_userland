/* date(1) - print the current time from gettimeofday(2) (S16).
 *
 * The kernel hands out whole seconds read from the CMOS RTC, which holds
 * local time but is reported as UTC until a timezone table exists.  There is
 * no gmtime(3)/strftime(3) in this libc subset, so the civil-date conversion
 * is done here (Howard Hinnant's days-to-civil algorithm).
 */
#include <stdio.h>
#include <sys/time.h>

static const char *wdays[7] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
static const char *months[12] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

int
main(int argc, char **argv)
{
	struct timeval tv;
	unsigned long t;
	int days, rem, z, era, doe, yoe, y, doy, mp, d, m;
	int hh, mm, ss;

	(void)argc;
	(void)argv;
	if (gettimeofday(&tv, 0) < 0) {
		printf("date: cannot read the clock\n");
		return (1);
	}
	t = (unsigned long)tv.tv_sec;
	days = (int)(t / 86400UL);
	rem = (int)(t % 86400UL);
	hh = rem / 3600;
	mm = (rem % 3600) / 60;
	ss = rem % 60;

	z = days + 719468;
	era = z / 146097;
	doe = z - era * 146097;
	yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	y = yoe + era * 400;
	doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	mp = (5 * doy + 2) / 153;
	d = doy - (153 * mp + 2) / 5 + 1;
	m = mp < 10 ? mp + 3 : mp - 9;
	if (m <= 2)
		y++;

	printf("%s %s %2d %02d:%02d:%02d UTC %d\n",
	    wdays[(days + 4) % 7], months[m - 1], d, hh, mm, ss, y);
	return (0);
}
