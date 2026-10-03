import java.awt.BorderLayout;
import java.awt.Color;
import java.awt.GridLayout;
import java.awt.Point;
import java.awt.event.KeyEvent;
import javax.swing.AbstractAction;
import javax.swing.BorderFactory;
import javax.swing.JButton;
import javax.swing.JComponent;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JSlider;
import javax.swing.KeyStroke;
import javax.swing.Timer;

public class ArduinoClientFinalVersion {

    // tracking stuff
    private static boolean lineMode = false;
    private static boolean mapAutoUpdate = false;
    private static String currentMovement = "none";
    private static int currentManoeuvre = 0;

    // stats
    private static long startTimeMs;
    private static double totalDistance = 0.0;
    private static Point lastPoint = null;

    public static void main(String[] args) throws Exception {

        startTimeMs = System.currentTimeMillis();

        JFrame window = new JFrame("RatBoy GUI");
        window.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        window.setLayout(new BorderLayout());

        final MapPanel mapPanel = new MapPanel();
        window.add(mapPanel, BorderLayout.CENTER);

        // info bar at top
        JPanel infoPanel = new JPanel(new GridLayout(2, 4, 10, 2));
        infoPanel.setBackground(new Color(50, 50, 70));
        infoPanel.setBorder(BorderFactory.createEmptyBorder(10, 10, 10, 10));

        final JLabel irLeftLabel = new JLabel("IR Left: (waiting...)");
        final JLabel irRightLabel = new JLabel("IR Right: (waiting...)");
        final JLabel distanceLabel = new JLabel("Distance: 0.00");
        final JLabel speedLabel = new JLabel("Speed: 0.00");
        final JLabel modeLabel = new JLabel("Mode: manual");
        final JLabel uptimeLabel = new JLabel("Uptime: 0s");
        final JLabel commandsLabel = new JLabel("Commands: 0");
        final JLabel pathLabel = new JLabel("Path: --");

        infoPanel.add(irLeftLabel);
        infoPanel.add(irRightLabel);
        infoPanel.add(distanceLabel);
        infoPanel.add(speedLabel);
        infoPanel.add(modeLabel);
        infoPanel.add(uptimeLabel);
        infoPanel.add(commandsLabel);
        infoPanel.add(pathLabel);

        window.add(infoPanel, BorderLayout.NORTH);

        JPanel controlPanel = new JPanel(new BorderLayout());
        controlPanel.setBackground(new Color(60, 60, 80));

        // movement buttons
        JPanel movePanel = new JPanel(new GridLayout(3, 3, 5, 5));
        movePanel.setBackground(new Color(60, 60, 80));
        movePanel.setBorder(BorderFactory.createEmptyBorder(10, 10, 10, 10));

        JButton manualButton = new JButton("manual mode");
        JButton lineButton = new JButton("line-following mode");
        JButton forward = new JButton("forward");
        JButton backward = new JButton("backward");
        JButton left = new JButton("left");
        JButton right = new JButton("right");
        JButton turnLeft = new JButton("turn left");
        JButton turnRight = new JButton("turn right");
        JButton stop = new JButton("stop");

        // so keyboard still works when clicking buttons
        for (JButton btn : new JButton[]
            {
                manualButton, lineButton, forward, backward,
                left, right, turnLeft, turnRight, stop
        })
        {
            btn.setFocusPainted(false);
            btn.setFocusable(false);
            btn.setFont(btn.getFont().deriveFont(12f));
        }

        // white text on dark background
        Color labelColor = new Color(220, 220, 220);
        for (JLabel label : new JLabel[]{
                irLeftLabel, irRightLabel, distanceLabel, speedLabel,
                modeLabel, uptimeLabel, commandsLabel, pathLabel
        })
        {
            label.setForeground(labelColor);
        }

        movePanel.add(manualButton);
        movePanel.add(forward);
        movePanel.add(lineButton);
        movePanel.add(left);
        movePanel.add(stop);
        movePanel.add(right);
        movePanel.add(turnLeft);
        movePanel.add(backward);
        movePanel.add(turnRight);

        controlPanel.add(movePanel, BorderLayout.CENTER);

        // manoeuvre buttons
        JPanel manoeuvrePanel = new JPanel(new GridLayout(1, 5, 5, 5));
        manoeuvrePanel.setBackground(new Color(60, 60, 80));
        manoeuvrePanel.setBorder(BorderFactory.createEmptyBorder(5, 10, 10, 10));

        JButton m1 = new JButton("m1");
        JButton m5 = new JButton("m5");
        JButton m6 = new JButton("m6");
        JButton resetMap = new JButton("reset map");
        JButton kidnapped = new JButton("kidnapped");

        m1.setToolTipText("reverse straight");
        m5.setToolTipText("parking");
        m6.setToolTipText("emergency stop");
        kidnapped.setToolTipText("find line when lost");

        for (JButton btn : new JButton[]{m1, m5, m6, resetMap, kidnapped}) {
            btn.setFocusable(false);
        }

        manoeuvrePanel.add(m1);
        manoeuvrePanel.add(m5);
        manoeuvrePanel.add(m6);
        manoeuvrePanel.add(resetMap);
        manoeuvrePanel.add(kidnapped);

        // sliders for tuning
        JPanel tuningPanel = new JPanel(new GridLayout(1, 3, 20, 5));
        tuningPanel.setBackground(new Color(60, 60, 80));
        tuningPanel.setBorder(BorderFactory.createEmptyBorder(10, 10, 10, 10));

        JPanel speedPanel = new JPanel(new BorderLayout(5, 2));
        speedPanel.setBackground(new Color(60, 60, 80));
        final JLabel speedValueLabel = new JLabel("speed: 100");
        speedValueLabel.setForeground(labelColor);
        JSlider speedSlider = new JSlider(50, 200, 100);
        speedSlider.setBackground(new Color(60, 60, 80));
        speedSlider.setFocusable(false);
        speedSlider.addChangeListener(e -> {
            int val = speedSlider.getValue();
            speedValueLabel.setText("speed: " + val);
            mapPanel.setSpeed(val);
            if (!speedSlider.getValueIsAdjusting()) {
                RobotController.sendCommand("/speed" + val);
            }
        });
        speedPanel.add(speedValueLabel, BorderLayout.NORTH);
        speedPanel.add(speedSlider, BorderLayout.CENTER);

        JPanel lostPanel = new JPanel(new BorderLayout(5, 2));
        lostPanel.setBackground(new Color(60, 60, 80));
        final JLabel lostValueLabel = new JLabel("lost threshold: 30");
        lostValueLabel.setForeground(labelColor);
        JSlider lostSlider = new JSlider(10, 100, 30);
        lostSlider.setBackground(new Color(60, 60, 80));
        lostSlider.setFocusable(false);
        lostSlider.addChangeListener(e -> {
            int val = lostSlider.getValue();
            lostValueLabel.setText("lost threshold: " + val);
            if (!lostSlider.getValueIsAdjusting()) {
                RobotController.sendCommand("/lost?val=" + val);
            }
        });
        lostPanel.add(lostValueLabel, BorderLayout.NORTH);
        lostPanel.add(lostSlider, BorderLayout.CENTER);

        JPanel endPanel = new JPanel(new BorderLayout(5, 2));
        endPanel.setBackground(new Color(60, 60, 80));
        final JLabel endValueLabel = new JLabel("end threshold: 50");
        endValueLabel.setForeground(labelColor);
        JSlider endSlider = new JSlider(20, 150, 50);
        endSlider.setBackground(new Color(60, 60, 80));
        endSlider.setFocusable(false);
        endSlider.addChangeListener(e -> {
            int val = endSlider.getValue();
            endValueLabel.setText("end threshold: " + val);
            if (!endSlider.getValueIsAdjusting()) {
                RobotController.sendCommand("/end?val=" + val);
            }
        });
        endPanel.add(endValueLabel, BorderLayout.NORTH);
        endPanel.add(endSlider, BorderLayout.CENTER);

        tuningPanel.add(speedPanel);
        tuningPanel.add(lostPanel);
        tuningPanel.add(endPanel);

        JPanel bottomSection = new JPanel(new BorderLayout());
        bottomSection.setBackground(new Color(60, 60, 80));
        bottomSection.add(manoeuvrePanel, BorderLayout.NORTH);
        bottomSection.add(tuningPanel, BorderLayout.SOUTH);

        controlPanel.add(bottomSection, BorderLayout.SOUTH);

        window.add(controlPanel, BorderLayout.SOUTH);

        // button handlers
        forward.addActionListener(e -> {
            RobotController.sendCommand("/forward");
            currentMovement = "forward";
        });

        backward.addActionListener(e -> {
            RobotController.sendCommand("/backward");
            currentMovement = "backward";
        });

        left.addActionListener(e -> {
            RobotController.sendCommand("/left");
            currentMovement = "left";
        });

        right.addActionListener(e -> {
            RobotController.sendCommand("/right");
            currentMovement = "right";
        });

        turnLeft.addActionListener(e -> {
            RobotController.sendCommand("/ccw");
            currentMovement = "ccw";
        });

        turnRight.addActionListener(e -> {
            RobotController.sendCommand("/cw");
            currentMovement = "cw";
        });

        stop.addActionListener(e -> {
            RobotController.sendCommand("/stop");
            currentMovement = "none";
            mapAutoUpdate = false;
            lastSensor = 0;
        });

        // mode buttons
        manualButton.addActionListener(e -> {
            lineMode = false;
            mapAutoUpdate = false;
            lastSensor = 0;
            RobotController.sendCommand("/stop");
            manualButton.setText("manual mode (active)");
            lineButton.setText("line-following mode");
        });

        lineButton.addActionListener(e -> {
            lineMode = true;
            mapAutoUpdate = true;
            lastSensor = 0;
            RobotController.sendCommand("/line");
            lineButton.setText("line-following mode (active)");
            manualButton.setText("manual mode");
        });

        // manoeuvre handlers
        m1.addActionListener(e -> {
            currentManoeuvre = 1;
            RobotController.sendCommand("/m1");
            pathLabel.setText("M1 - Reverse");
        });

        m5.addActionListener(e -> {
            currentManoeuvre = 5;
            RobotController.sendCommand("/m5");
            pathLabel.setText("M5 - Parking");
        });

        m6.addActionListener(e -> {
            currentManoeuvre = 6;
            RobotController.sendCommand("/m6");
            pathLabel.setText("M6 - Emergency Stop");
        });

        // reset the map
        resetMap.addActionListener(e -> {
            mapPanel.reset();
            totalDistance = 0.0;
            lastPoint = null;
        });

        // kidnapped mode
        kidnapped.addActionListener(e -> {
            RobotController.sendCommand("/kidnapped");
            pathLabel.setText("Kidnapped mode");
        });

        // keyboard controls
        var inputMap = window.getRootPane().getInputMap(JComponent.WHEN_IN_FOCUSED_WINDOW);
        var actionMap = window.getRootPane().getActionMap();

        inputMap.put(KeyStroke.getKeyStroke(KeyEvent.VK_W, 0), "forward");
        inputMap.put(KeyStroke.getKeyStroke(KeyEvent.VK_S, 0), "backward");
        inputMap.put(KeyStroke.getKeyStroke(KeyEvent.VK_A, 0), "left");
        inputMap.put(KeyStroke.getKeyStroke(KeyEvent.VK_D, 0), "right");
        inputMap.put(KeyStroke.getKeyStroke(KeyEvent.VK_Q, 0), "turnLeft");
        inputMap.put(KeyStroke.getKeyStroke(KeyEvent.VK_E, 0), "turnRight");
        inputMap.put(KeyStroke.getKeyStroke(KeyEvent.VK_SPACE, 0), "stop");

        actionMap.put("forward", new AbstractAction() {
            @Override
            public void actionPerformed(java.awt.event.ActionEvent e) {
                RobotController.sendCommand("/forward");
                currentMovement = "forward";
            }
        });
        actionMap.put("backward", new AbstractAction() {
            @Override
            public void actionPerformed(java.awt.event.ActionEvent e) {
                RobotController.sendCommand("/backward");
                currentMovement = "backward";
            }
        });
        actionMap.put("left", new AbstractAction() {
            @Override
            public void actionPerformed(java.awt.event.ActionEvent e) {
                RobotController.sendCommand("/left");
                currentMovement = "left";
            }
        });
        actionMap.put("right", new AbstractAction() {
            @Override
            public void actionPerformed(java.awt.event.ActionEvent e) {
                RobotController.sendCommand("/right");
                currentMovement = "right";
            }
        });
        actionMap.put("turnLeft", new AbstractAction() {
            @Override
            public void actionPerformed(java.awt.event.ActionEvent e) {
                RobotController.sendCommand("/ccw");
                currentMovement = "ccw";
            }
        });
        actionMap.put("turnRight", new AbstractAction() {
            @Override
            public void actionPerformed(java.awt.event.ActionEvent e) {
                RobotController.sendCommand("/cw");
                currentMovement = "cw";
            }
        });
        actionMap.put("stop", new AbstractAction() {
            @Override
            public void actionPerformed(java.awt.event.ActionEvent e) {
                RobotController.sendCommand("/stop");
                currentMovement = "none";
                mapAutoUpdate = false;
                lastSensor = 0;
            }
        });

        // update map position
        Timer moveTimer = new Timer(100, e -> {
            if (lineMode) return;

            switch (currentMovement) {
                case "forward":
                    mapPanel.moveForward();
                    break;
                case "backward":
                    mapPanel.moveBackward();
                    break;
                case "left":
                    mapPanel.moveLeft();
                    break;
                case "right":
                    mapPanel.moveRight();
                    break;
                case "ccw":
                    mapPanel.rotateLeftSmall();
                    break;
                case "cw":
                    mapPanel.rotateRightSmall();
                    break;
            }
        });
        moveTimer.start();

        // poll sensors and update ui
        Timer irTimer = new Timer(200, e -> {

            String data = RobotController.getInfraredData();

            if (data == null) {
                irLeftLabel.setText("IR Left: ERROR");
                irRightLabel.setText("IR Right: ERROR");
            } else {
                String[] parts = data.split(",");
                if (parts.length == 2) {
                    irLeftLabel.setText("IR Left: " + parts[0].trim());
                    irRightLabel.setText("IR Right: " + parts[1].trim());
                } else {
                    irLeftLabel.setText("IR Left: " + data);
                    irRightLabel.setText("IR Right: ?");
                }

                if (lineMode && mapAutoUpdate) {
                    updateMapFromIR(mapPanel, data);
                }
            }

            double stepDist = 0.0;
            Point current = mapPanel.getRobotPoint();
            if (lastPoint != null) {
                double dx = current.x - lastPoint.x;
                double dy = current.y - lastPoint.y;
                stepDist = Math.sqrt(dx * dx + dy * dy);
                totalDistance += stepDist;
            }
            lastPoint = current;

            distanceLabel.setText("Distance: " + String.format("%.2f", totalDistance));

            long now = System.currentTimeMillis();
            long uptime = (now - startTimeMs) / 1000;
            uptimeLabel.setText("Uptime: " + uptime + "s");

            modeLabel.setText("Mode: " + (lineMode ? "line-following" : "manual"));

            // speed is distance per tick (200ms)
            double speed = stepDist / 0.2;
            speedLabel.setText("Speed: " + String.format("%.2f", speed));

            commandsLabel.setText("Commands: " + RobotController.getCommandCount());
        });
        irTimer.start();

        window.pack();
        window.setVisible(true);
    }

    // for tracking line following on the map
    private static int lastSensor = 0;  // -1 left, 0 none, 1 right
    private static final int LINE_THRESHOLD = 100;

    private static void updateMapFromIR(MapPanel map, String data) {
        try {
            String[] p = data.split(",");
            if (p.length != 2) return;

            int L = Integer.parseInt(p[0].trim());
            int R = Integer.parseInt(p[1].trim());
            
            boolean leftOnLine = L > LINE_THRESHOLD;
            boolean rightOnLine = R > LINE_THRESHOLD;

            // turning left, wait for left sensor
            if (lastSensor == -1) {
                map.smoothRotateBy(5);
                if (leftOnLine) { 
                    lastSensor = 0; 
                    map.moveForward(); 
                }
                return;
            }
            
            // turning right, wait for right sensor
            if (lastSensor == 1) {
                map.smoothRotateBy(-5);
                if (rightOnLine) { 
                    lastSensor = 0; 
                    map.moveForward(); 
                }
                return;
            }

            // both off = on line, go forward
            if (!leftOnLine && !rightOnLine) {
                map.moveForward();
                return;
            }

            // left triggered
            if (leftOnLine && !rightOnLine) {
                lastSensor = -1;
                map.smoothRotateBy(5);
                return;
            }
            
            // right triggered
            if (!leftOnLine && rightOnLine) {
                lastSensor = 1;
                map.smoothRotateBy(-5);
                return;
            }
            
            // both on = intersection
            if (leftOnLine && rightOnLine) {
                map.moveForward();
            }

        } catch (Exception ex) {
            // parsing failed
        }
    }
}
