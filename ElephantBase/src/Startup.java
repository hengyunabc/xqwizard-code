import net.elephantbase.util.ClassPath;
import net.elephantbase.util.server.JettyServer;

public class Startup {
	private static final ClassPath DEFAULT_HOME = ClassPath.getInstance().
			append("../../../..");

	public static void main(String[] args) throws Exception {
		int port = 8080;
		if (args.length > 0) {
			try {
				port = Integer.parseInt(args[0]);
			} catch (Exception e) {
				// Ignored if "port" cannot be parsed
			}
		}
		JettyServer server = new JettyServer(DEFAULT_HOME, port);
		server.start();
		server.join();
	}
}