import numpy as np
import matplotlib.pyplot as plt

# Small file to design IIR filter ot block DC component

def filter_z(z, beta=0.9):
    return (1-z**(-1))/(1-beta*z**(-1))

def filter_gain_compensation(z, beta=0.9):
    return (1+beta)/2*filter_z(z, beta)

fs = 125

f = np.logspace(-2, np.log10(fs/2), 8000)  # Frequency range from 0.1 Hz to Nyquist frequency
omega = 2*np.pi*f/fs

H1 = filter_gain_compensation(np.exp(1j*omega), beta=0.99)
H2 = filter_gain_compensation(np.exp(1j*omega), beta=0.995)
H3 = filter_gain_compensation(np.exp(1j*omega), beta=0.999)

print("Cutoff frequency (3dB point):", f[np.argmin(np.abs(20*np.log10(np.abs(H2)) + 3))], "Hz")

plt.rcParams.update({'font.size': 16})
plt.plot(f, 20*np.log10(np.abs(H1)), '--', label='$\\beta=0.99$', color='blue')
plt.plot(f, 20*np.log10(np.abs(H2)), '--', label='$\\beta=0.995$', color='red')
plt.plot(f, 20*np.log10(np.abs(H3)), '--', label='$\\beta=0.999$', color='green')
plt.xlabel('Frequency (Hz)', fontsize=14)
plt.xscale('log')
plt.ylabel('Gain (dB)', fontsize=14)
plt.ylim(bottom=-20)
plt.legend(fontsize=12)
plt.grid(which='both', linestyle='--', alpha=0.5)
ax = plt.gca()
ax.tick_params(axis='both', which='major', labelsize=12)
plt.savefig('dc_block_filter_response.pdf', bbox_inches='tight', dpi=300)
plt.show()
