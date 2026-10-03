import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.time.Duration;

public class RobotController {

    // set to true for testing without robot
    private static final boolean OFFLINE = false;
    private static int commandCount = 0;

    // send command to robot
    public static void sendCommand(String path) {

        commandCount++;

        if (OFFLINE) {
            System.out.println("[offline] " + path);
            return;
        }

        try {
            String url = "http://192.168.1.1" + path;

            HttpClient client = HttpClient.newBuilder()
                    .connectTimeout(Duration.ofSeconds(5))
                    .build();

            HttpRequest req = HttpRequest.newBuilder()
                    .uri(URI.create(url))
                    .timeout(Duration.ofSeconds(10))
                    .GET()
                    .build();

            HttpResponse<String> res = client.send(req, HttpResponse.BodyHandlers.ofString());
            System.out.println("Body: " + res.body());

        } catch (Exception ex) {
            System.out.println("command failed");
        }
    }

    public static String getInfraredData() {

        if (OFFLINE) {
            return "0,0";
        }

        try {
            String url = "http://192.168.1.1/infrared";

            HttpClient client = HttpClient.newBuilder()
                    .connectTimeout(Duration.ofSeconds(5))
                    .build();

            HttpRequest req = HttpRequest.newBuilder()
                    .uri(URI.create(url))
                    .timeout(Duration.ofSeconds(10))
                    .GET()
                    .build();

            HttpResponse<String> res = client.send(req, HttpResponse.BodyHandlers.ofString());
            System.out.println("infrared body: " + res.body());
            return res.body();

        } catch (Exception ex) {
            System.out.println("ir read failed");
            return null; 
        }
    }

    public static int getCommandCount() {
        return commandCount;
    }

    // get data from robot
    public static String getData(String path) {
        if (OFFLINE) {
            if (path.contains("speed")) return "100";
            if (path.contains("lost")) return "30";
            if (path.contains("end")) return "50";
            return "0";
        }

        try {
            String url = "http://192.168.1.1" + path;

            HttpClient client = HttpClient.newBuilder()
                    .connectTimeout(Duration.ofSeconds(5))
                    .build();

            HttpRequest req = HttpRequest.newBuilder()
                    .uri(URI.create(url))
                    .timeout(Duration.ofSeconds(10))
                    .GET()
                    .build();

            HttpResponse<String> res = client.send(req, HttpResponse.BodyHandlers.ofString());
            return res.body();

        } catch (Exception ex) {
            System.out.println("getData failed");
            return null;
        }
    }
}
