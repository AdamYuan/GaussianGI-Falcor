# 3QGS Trainer

$C = \sum_{i=1}^n C_i, C_i = c_i \alpha_i T_i, T_i = \prod_{j=1}^{i-1}(1-\alpha_j)$

$T = \prod_{i=1}^n(1-\alpha_i)$

$C = \sum_{i=1}^n [c_i \alpha_i \prod_{j=1}^{i-1}(1-\alpha_j)], C_i = c_i \alpha_i \prod_{j=1}^{i-1}(1-\alpha_j)$

$\dfrac{dL}{dc_i}=\dfrac{dL}{dC}\dfrac{dC}{dc_i} = \dfrac{dL}{dC} \alpha_i T_i$

$\begin{aligned}\dfrac{dL}{d\alpha_i} &= \dfrac{dL}{dC}\dfrac{dC}{d\alpha_i} = \dfrac{dL}{dC}\sum_{j=i}^n \dfrac{dC}{dC_j}\dfrac{dC_j}{d\alpha_i} \\ &= \dfrac{dL}{dC}(c_i T_i +  \frac{\sum_{j=i+1}^n C_j}{\alpha_i - 1}) \\ &= \dfrac{dL}{dC}(c_i T_i +  \frac{C - \sum_{j=1}^i C_j}{\alpha_i - 1}) \\ &= \dfrac{dL}{dC}T_i(c_i - \sum_{j=i+1}^n \alpha_j c_j \frac{T_j}{T_{i + 1}})\end{aligned}$



$Q \text{ is QuadPos}$

$Q = \begin{bmatrix}Q_0 \\ Q_1\end{bmatrix}, P = \begin{bmatrix}P_1 \\ P_2\end{bmatrix}, \mu = \begin{bmatrix}\mu_1 \\ \mu_2\end{bmatrix}$

$A = \begin{bmatrix}S_0A_x& -S_1A_y\\S_0A_y & S_1 A_x\end{bmatrix}, A_x^2 + A_y^2 = 1, A^{-1}=\begin{bmatrix}\frac{A_x}{S_0}& \frac{A_y}{S_0}\\-\frac{A_y}{S_1} & \frac{A_x}{S_1}\end{bmatrix}$

$\alpha = e^{-Q^2}, P = \text{const} = p + AQ \Rightarrow Q = A^{-1}(P - \mu)$

$\dfrac{dL}{dA}=\dfrac{dL}{d\alpha}\dfrac{d\alpha}{dQ}\dfrac{dQ}{dA^{-1}}\dfrac{dA^{-1}}{dA}$

$\dfrac{dL}{d\mu} = \dfrac{dL}{d\alpha}\dfrac{d\alpha}{dQ}\dfrac{dQ}{d\mu}$