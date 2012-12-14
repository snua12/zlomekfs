class Circle {
public:
    Circle(void) : m_iX(0), m_iY(0), m_iR(1), m_ulColor(0) {}
    void SetXPosition(int X) { m_iX = X; }
    int GetXPosition(void) const { return m_iX; }
    void SetYPosition(int Y) { m_iY = Y; }
    int GetYPosition(void) const { return m_iY; }
    void SetRadius(int R) { m_iR = R; }
    int GetRadius(void) const { return m_iR; }
    void SetColor(unsigned long C) { m_ulColor = C; }
    unsigned long GetColor(void) const { return m_ulColor; }
private:
    int m_iX;
    int m_iY;
    int m_iR;
    unsigned long m_ulColor;
};
void Draw(const Circle& C) {}
