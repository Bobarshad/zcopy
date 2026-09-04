---
1- How did you calculate these numbers?
---

Hardware Power Floor (Limited Range): 
Modern Wi-Fi 7 chipsets typically clamp their minimum stable power around +5 dBm to +6 dBm.
You cannot software-command the radio down to
$-30\text{ dBm}$ or $-50\text{ dBm}$. 
If the two devices sit on the same desk 
(e.g., $1\text{ meter}$ apart), 
free-space path loss at 6 GHz is only
$\approx 48\text{ dB}$. 
Even at $+6\text{ dBm}$, 
the received signal remains around 
$-42\text{ dBm}$—which is still strong enough for high-order modulation.


The calculation relies on the standard **Free-Space Path Loss (FSPL)** equation used in RF engineering, paired with common hardware constraints of Wi-Fi power amplifier (PA) front-ends.

---

### 1. Free-Space Path Loss ($\text{FSPL} \approx 48\text{ dB}$)

Free-space path loss represents the attenuation of an electromagnetic wave traveling through unobstructed space. The formula is:

$$\text{FSPL} = 20\log_{10}(d) + 20\log_{10}(f) + 20\log_{10}\left(\frac{4\pi}{c}\right)$$

Where:

* $d = 1\text{ m}$ (distance between R1 and R2 on a desk)
* $f = 6.5\times 10^9\text{ Hz}$ (center of the 6 GHz UNII band)
* $c \approx 3\times 10^8\text{ m/s}$ (speed of light in a vacuum)

Using the simplified practical formula where distance $d$ is in **meters** and frequency $f$ is in **megahertz (MHz)**:

$$\text{FSPL (dB)} = 20\log_{10}(d_{\text{meters}}) + 20\log_{10}(f_{\text{MHz}}) - 27.55$$

Plugging in $d = 1\text{ m}$ and $f = 6500\text{ MHz}$:

$$\text{FSPL} = 20\log_{10}(1) + 20\log_{10}(6500) - 27.55$$

$$\text{FSPL} = 0 + 20(3.8129) - 27.55$$

$$\text{FSPL} = 76.26 - 27.55 \approx 48.71\text{ dB} \approx 48\text{ dB}$$

---

### 2. Received Signal Calculation ($\text{RSSI} \approx -42\text{ dBm}$)

The received power ($P_{\text{rx}}$) follows the link budget equation:

$$P_{\text{rx}} = P_{\text{tx}} + G_{\text{tx}} + G_{\text{rx}} - \text{FSPL}$$

Assuming unity/nominal isotropic antenna gains for close-proximity bench testing ($G_{\text{tx}} \approx 0\text{ dBi}$, $G_{\text{rx}} \approx 0\text{ dBi}$):

$$P_{\text{rx}} = +6\text{ dBm} + 0\text{ dBi} + 0\text{ dBi} - 48\text{ dB} = -42\text{ dBm}$$

Even with typical omnidirectional dipole antennas ($2\text{ dBi}$ to $3\text{ dBi}$ each), the signal would be slightly stronger (around $-38\text{ dBm}$ to $-36\text{ dBm}$).

---

### 3. Why $-42\text{ dBm}$ Still Produces High-Order Modulation

In 802.11be (Wi-Fi 7):

* **Noise Floor:** Thermal noise over a wide 320 MHz channel is roughly $-174\text{ dBm/Hz} + 10\log_{10}(320\times 10^6) \approx -89\text{ dBm}$. With a receiver noise figure of $\approx 6\text{ dB}$, the effective noise floor is roughly **$-83\text{ dBm}$**.
* **SNR at $-42\text{ dBm}$:**

$$\text{SNR} = -42\text{ dBm} - (-83\text{ dBm}) = 41\text{ dB}$$


* **Sensitivity Thresholds:** Wi-Fi 7 MCS 12/13 (4096-QAM) generally requires an SNR of roughly **$36\text{ to }40\text{ dB}$** (a received power around $-45\text{ dBm}$ or higher depending on implementation). Because $41\text{ dB} > 36\text{ dB}$, the radio easily stays pinned at top-tier modulations.

---

### 4. Why Chipsets Clamp at $+5\text{ to }+6\text{ dBm}$

Wi-Fi Front-End Modules (FEMs) incorporate a Power Amplifier (PA) designed to operate linearly between $\approx +10\text{ dBm}$ and $+24\text{ dBm}$.

* When you run `iw dev <wlan> set txpower fixed 0`, the driver/firmware limits the minimum digital attenuation and PA gain steps to prevent the amplifier from falling into an uncalibrated, non-linear operating state.
* The Qualcomm (`ath12k`) and MediaTek (`mt7996`) drivers typically clamp the lowest programmable target power floor in their regulatory and calibration look-up tables around **$5\text{ to }6\text{ dBm}$**, ignoring lower arbitrary requests.


---
2- What is +6 dBm in the following calculation? Is it the chipset's clamp at $+5\text{ to }+6\text{ dBm}$?

$$P_{\text{rx}} = +6\text{ dBm} + 0\text{ dBi} + 0\text{ dBi} - 48\text{ dB} = -42\text{ dBm}$$
---


**Yes, exactly.** In that formula, **$+6\text{ dBm}$** represents the transmit power ($P_{\text{tx}}$) set to that hardware clamping floor.

Here is how each term in the link budget maps out:

* **$P_{\text{tx}} = +6\text{ dBm}$:** The lowest possible transmit power the Wi-Fi 7 chipset/FEM can physically output after firmware clamps your `iw set txpower` command.
* **$G_{\text{tx}} = 0\text{ dBi}$:** The transmit antenna gain of R1 (assumed nominal/isotropic for bench modeling).
* **$G_{\text{rx}} = 0\text{ dBi}$:** The receive antenna gain of R2 (assumed nominal/isotropic).
* **$\text{FSPL} = 48\text{ dB}$:** The free-space path loss over the 1-meter distance at 6 GHz.
* **$P_{\text{rx}} = -42\text{ dBm}$:** The resulting signal strength (RSSI) arriving at R2's receiver.

The calculation illustrates the core limitation: even when you instruct the router to transmit at the lowest power the hardware allows ($+6\text{ dBm}$), the signal reaching R2 from 1 meter away remains around $-42\text{ dBm}$, which is still far too loud to force Wi-Fi 7 to drop down into lower MCS tiers.


---
3- Is dBi = dBm = dB?
---

To verify whether $\text{dBi} = \text{dBm} = \text{dB}$, consider how each unit is defined mathematically and what physical quantity it represents:

1. **$\text{dB}$ (Decibel):**
* A dimensionless, relative ratio between two values of the same unit (such as power, voltage, or sound pressure).
* Formula for a power ratio:

$$\text{dB} = 10 \log_{10}\left(\frac{P_1}{P_2}\right)$$


* It measures relative gain or loss, not an absolute quantity.


2. **$\text{dBm}$ (Decibels relative to 1 milliwatt):**
* An absolute unit of power referenced to $1\text{ milliwatt}$ ($1\text{ mW}$).
* Formula:

$$\text{dBm} = 10 \log_{10}\left(\frac{P}{1\text{ mW}}\right)$$


* Example: $0\text{ dBm} = 1\text{ mW}$, $10\text{ dBm} = 10\text{ mW}$, $20\text{ dBm} = 100\text{ mW}$.


3. **$\text{dBi}$ (Decibels relative to an isotropic radiator):**
* A measure of antenna directional gain relative to a theoretical, ideal isotropic antenna that radiates power equally in all directions with $0\text{ dBi}$ gain.
* It measures directional focus or gain, not total RF power output.



Because each term uses a completely different reference point, represents a different physical dimension (dimensionless ratio vs. absolute power vs. directional antenna gain), and cannot be converted without an external parameter, $\text{dBi} \neq \text{dBm} \neq \text{dB}$.

Therefore, the statement is incorrect.



---
4- But when we write it like:

Prx = +6 dBm + 0 dBi + 0 dBi - 48 dB = -42 dBm

Can we sum numbers with different units? 
---


To determine whether you can sum these numbers, examine the underlying mathematics of the logarithmic scale and what each term physically represents:

In linear power units (watts or milliwatts), a wireless link budget is calculated by **multiplying** and **dividing** factors:

$$P_{\text{rx}}\,(\text{mW}) = P_{\text{tx}}\,(\text{mW}) \times g_{\text{tx}}\,(\text{ratio}) \times g_{\text{rx}}\,(\text{ratio}) \times \frac{1}{L\,(\text{ratio})}$$

Notice the units in linear terms:

* $P_{\text{tx}}$ is an **absolute power** (in $\text{mW}$).
* $g_{\text{tx}}$, $g_{\text{rx}}$, and $L$ are **dimensionless multipliers/ratios** (scaling factors).
* The result, $P_{\text{rx}}$, retains the unit of absolute power: $\text{mW} \times 1 \times 1 / 1 = \text{mW}$.

When you take the base-10 logarithm ($10\log_{10}$) of both sides to convert to decibels, multiplication turns into addition and division turns into subtraction:

$$10\log_{10}\left(\frac{P_{\text{rx}}}{1\text{ mW}}\right) = 10\log_{10}\left(\frac{P_{\text{tx}}}{1\text{ mW}}\right) + 10\log_{10}(g_{\text{tx}}) + 10\log_{10}(g_{\text{rx}}) - 10\log_{10}(L)$$

Now, map the labels to each component:

* $10\log_{10}\left(\frac{P_{\text{tx}}}{1\text{ mW}}\right)$ is labeled **$\text{dBm}$** to remind you it is referenced to $1\text{ mW}$.
* $10\log_{10}(g_{\text{tx}})$ is labeled **$\text{dBi}$** to remind you it is an antenna gain ratio referenced to an isotropic radiator.
* $10\log_{10}(L)$ is labeled **$\text{dB}$** to indicate it is a pure dimensionless loss ratio.

Even though they have different descriptive suffixes ($\text{m}$, $\text{i}$, or none), **$\text{dB}$ and $\text{dBi}$ are pure dimensionless numbers** added to a logarithmic scale.

Just like multiplying $6\text{ mW} \times 2 = 12\text{ mW}$ is mathematically valid because $2$ is a dimensionless multiplier, adding $+6\text{ dBm} + 3\text{ dB} = +9\text{ dBm}$ is mathematically sound.

The arithmetic rules for decibel operations are:

* $\text{Absolute Power (dBm)} + \text{Gain/Loss Ratio (dB or dBi)} = \text{Absolute Power (dBm)}$
* $\text{Absolute Power (dBm)} - \text{Absolute Power (dBm)} = \text{Relative Ratio (dB)}$
* $\text{Absolute Power (dBm)} + \text{Absolute Power (dBm)}$ is **undefined** (you cannot add two decibel-milliwatts directly; you must convert to linear watts first).

Therefore, writing $+6\text{ dBm} + 0\text{ dBi} + 0\text{ dBi} - 48\text{ dB} = -42\text{ dBm}$ is completely valid mathematically and standard practice in RF engineering.





