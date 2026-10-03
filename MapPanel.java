import java.awt.BasicStroke;
import java.awt.Color;
import java.awt.Dimension;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.Point;
import java.awt.RenderingHints;
import java.util.ArrayList;
import java.util.List;
import javax.swing.JPanel;

public class MapPanel extends JPanel {

    // position tracking
    private List<Point> trail;
    private double robotX, robotY;
    private double robotAngle = 0.0;
    
    private int speed = 100;
    private static final int BASE = 100;
    private static final double STEP = 5.0;
    private static final double ROT_STEP = 3.0;
    private static final int MARGIN = 30;
    private static final int MAX_TRAIL = 1000;
    
    // rotation stuff
    private double accumRot = 0.0;
    private boolean rotating = false;
    private double angVel = 0.0;
    private static final double MAX_ANG_VEL = 45.0;
    private long lastTime = System.currentTimeMillis();

    public MapPanel() {
        setBackground(Color.WHITE);
        setPreferredSize(new Dimension(400, 400));
        robotX = 0;
        robotY = 0;
        trail = new ArrayList<>();
        trail.add(new Point(0, 0));
    }

    public void reset() {
        robotX = 0;
        robotY = 0;
        robotAngle = 0.0;
        trail.clear();
        trail.add(new Point(0, 0));
        repaint();
    }

    public void setSpeed(int s) {
        this.speed = Math.max(50, Math.min(200, s));
    }

    public int getSpeed() { return speed; }

    private double getStep() { return STEP * speed / (double) BASE; }
    private double getRotStep() { return ROT_STEP * speed / (double) BASE; }

    // movement
    public void moveForward() {
        double s = getStep();
        double rad = Math.toRadians(robotAngle);
        robotX -= s * Math.sin(rad);
        robotY -= s * Math.cos(rad);
        addTrailPoint();
    }

    public void moveBackward() {
        double s = getStep();
        double rad = Math.toRadians(robotAngle);
        robotX += s * Math.sin(rad);
        robotY += s * Math.cos(rad);
        addTrailPoint();
    }

    // strafe (mecanum)
    public void moveLeft() {
        double s = STEP * (speed + 40) / (double) BASE;
        double rad = Math.toRadians(robotAngle + 90);
        robotX -= s * Math.sin(rad);
        robotY -= s * Math.cos(rad);
        addTrailPoint();
    }

    public void moveRight() {
        double s = STEP * (speed + 40) / (double) BASE;
        double rad = Math.toRadians(robotAngle - 90);
        robotX -= s * Math.sin(rad);
        robotY -= s * Math.cos(rad);
        addTrailPoint();
    }

    // smooth 90 degree rotations with acceleration
    public void rotateLeft() {
        if (!rotating) {
            rotating = true;
            accumRot = 0.0;
            angVel = 0.0;
            lastTime = System.currentTimeMillis();
        }
        doRotateLeft();
    }

    public void rotateRight() {
        if (!rotating) {
            rotating = true;
            accumRot = 0.0;
            angVel = 0.0;
            lastTime = System.currentTimeMillis();
        }
        doRotateRight();
    }

    private void doRotateLeft() {
        long now = System.currentTimeMillis();
        double dt = (now - lastTime) / 1000.0;
        lastTime = now;

        double target = -MAX_ANG_VEL;
        double accel = (target - angVel) / 0.1;
        angVel += accel * dt;
        angVel = Math.max(angVel, target);

        double delta = angVel * dt;
        accumRot += Math.abs(delta);
        robotAngle += delta;

        if (accumRot >= 90.0) {
            robotAngle = Math.round(robotAngle / 90.0) * 90.0;
            accumRot = 0.0;
            rotating = false;
            angVel = 0.0;
        }
        repaint();
    }

    private void doRotateRight() {
        long now = System.currentTimeMillis();
        double dt = (now - lastTime) / 1000.0;
        lastTime = now;

        double target = MAX_ANG_VEL;
        double accel = (target - angVel) / 0.1;
        angVel += accel * dt;
        angVel = Math.min(angVel, target);

        double delta = angVel * dt;
        accumRot += Math.abs(delta);
        robotAngle += delta;

        if (accumRot >= 90.0) {
            robotAngle = Math.round(robotAngle / 90.0) * 90.0;
            accumRot = 0.0;
            rotating = false;
            angVel = 0.0;
        }
        repaint();
    }

    // add point to trail
    private void addTrailPoint() {
        trail.add(new Point((int) robotX, (int) robotY));
        if (trail.size() > MAX_TRAIL) {
            trail.remove(0);
        }
        repaint();
    }

    public Point getRobotPoint() {
        return new Point((int) robotX, (int) robotY);
    }

    public void rotateLeftSmall() { robotAngle += getRotStep(); repaint(); }
    public void rotateRightSmall() { robotAngle -= getRotStep(); repaint(); }

    // gradual turns
    public void accumulateRotation(double deg) {
        accumRot += Math.abs(deg);
        robotAngle += deg;
        if (accumRot >= 90.0) {
            robotAngle = Math.round(robotAngle / 90.0) * 90.0;
            accumRot = 0.0;
        }
        repaint();
    }

    // smooth rotation
    public void smoothRotateBy(double deg) {
        double smooth = 1.0 - Math.exp(-Math.abs(deg) / 15.0);
        robotAngle += deg * smooth;
        
        while (robotAngle < 0) robotAngle += 360;
        while (robotAngle >= 360) robotAngle -= 360;
        repaint();
    }

    @Override
    protected void paintComponent(Graphics g) {
        super.paintComponent(g);

        // drawing
        Graphics2D g2d = (Graphics2D) g;
        g2d.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);

        if (trail.isEmpty()) return;

        int cx = getWidth() / 2;
        int cy = getHeight() / 2;

        // border
        g2d.setColor(new Color(100, 100, 120));
        g2d.setStroke(new BasicStroke(2));
        g2d.drawRect(MARGIN, MARGIN, getWidth() - 2 * MARGIN, getHeight() - 2 * MARGIN);

        // grid
        g2d.setColor(new Color(220, 220, 230));
        int grid = 50;
        int offX = (int) (robotX % grid);
        int offY = (int) (robotY % grid);
        for (int i = -6; i <= 6; i++) {
            g2d.drawLine(cx + i * grid - offX, cy - 6 * grid - offY, cx + i * grid - offX, cy + 6 * grid - offY);
            g2d.drawLine(cx - 6 * grid - offX, cy + i * grid - offY, cx + 6 * grid - offX, cy + i * grid - offY);
        }

        // trail
        g2d.setColor(new Color(150, 150, 200));
        g2d.setStroke(new BasicStroke(2));
        for (int i = 1; i < trail.size(); i++) {
            Point p1 = trail.get(i - 1);
            Point p2 = trail.get(i);
            int x1 = cx + (int) (p1.x - robotX);
            int y1 = cy + (int) (p1.y - robotY);
            int x2 = cx + (int) (p2.x - robotX);
            int y2 = cy + (int) (p2.y - robotY);
            g2d.drawLine(x1, y1, x2, y2);
        }

        // robot (blue square with orange direction arrow)
        g2d.setColor(new Color(0, 100, 200));
        int sz = 10;
        g2d.fillRect(cx - sz / 2, cy - sz / 2, sz, sz);

        double rad = Math.toRadians(robotAngle);
        int len = 20;
        int ax = cx - (int) (len * Math.sin(rad));
        int ay = cy - (int) (len * Math.cos(rad));
        g2d.setColor(new Color(255, 100, 0));
        g2d.setStroke(new BasicStroke(2));
        g2d.drawLine(cx, cy, ax, ay);

        g2d.setColor(new Color(255, 200, 0));
        g2d.drawOval(cx - sz / 2 - 3, cy - sz / 2 - 3, sz + 6, sz + 6);
    }
}
