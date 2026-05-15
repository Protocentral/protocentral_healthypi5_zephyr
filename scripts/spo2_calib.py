import numpy as np
import matplotlib.pyplot as plt

R = np.linspace(0.5,1.3, 500)

spo2_1 = 1.5958422 * R**2 - 34.6596622 * R + 112.6898759
spo2_2 = -16.666666*R**2 + 8.333333*R + 100
spo2_healthy_pi = -45.060*R**2 + 30.354*R + 94.845 #Formula: SpO2 = -45.060*R² + 30.354*R + 94.845

plt.plot(R, spo2_1, label="MAX30101")
plt.plot(R, spo2_2, label="MAX86140/MAX86141")
plt.plot(R, spo2_healthy_pi, label="Healthy PI")
plt.xlabel("R (IR/Red ratio)")
plt.ylabel("SpO2 (%)")
plt.legend()
plt.grid()
plt.show()
