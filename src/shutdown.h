/**
 *  Self-pipe trick, see <http://cr.yp.to/docs/selfpipe.html>.
 */
namespace Shutdown {
// When data is available to read on fd_in, the program is supposed to
// shutdown.
extern int fd_in;
// Returns 0 if successful, -1 in case of a fatal error.
int init();
// Request to terminate the program by making data available to read on fd_in.
void exit();
}  // namespace Shutdown
