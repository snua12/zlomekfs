public class Circle {
    public Circle() { m_iX = 0; m_iY = 0; m_iR = 1; m_ulColor = 0; }
    public void setXPosition(int X) { m_iX = X; }
    public int getXPosition() { return m_iX; }
    public void setYPosition(int Y) { m_iY = Y; }
    public int getYPosition() { return m_iY; }
    public void setRadius(int R) { m_iR = R; }
    public int getRadius() { return m_iR; }
    public void setColor(long C) { m_ulColor = C; }
    public long getColor() { return m_ulColor; }
    public void draw() {}

    private int m_iX;
    private int m_iY;
    private int m_iR;
    private long m_ulColor;
}
