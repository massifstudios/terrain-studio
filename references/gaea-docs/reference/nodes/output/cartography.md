Source: https://docs.gaea.app/reference/nodes/output/cartography.html

# Cartography

Output › Render

Shortcode `crt`

Cartography creates a topographical overlay for a map.

## When to Use It

Use Cartography when you need a topographical overlay for a map.

## Properties

**Accuracy** (`Single`)  
Controls the precision of map features such as contours and gradients. Higher values produce more detailed and accurate representations of terrain.

**Gradient Style** (`Enum`)  
Specifies the style of gradient used to represent elevation or terrain features on the map.

- **Smooth** — Applies a continuous, smooth gradient for a natural transition between elevation bands.
- **Large Contours** — Emphasizes major elevation bands with broad, distinct contour regions.
- **All Contours** — Displays all contour lines, including minor and major bands, for maximum detail.

**Equalize Gradient** (`Boolean`)  
Balances the gradient distribution to ensure even representation of elevation changes across the map.

**Reverse** (`Boolean`)  
Inverts the color or gradient mapping, swapping high and low elevation colors for a different visual effect.

**Erode** (`Boolean`)  
Applies an erosion effect to the map, softening features and simulating natural terrain wear.

**Land Palette** (`Enum`)  
Selects the color palette used for land areas on the map.

- **Classic** — Uses a traditional cartographic color scheme for land.
- **Minimal** — Applies a minimalistic color palette for a clean, modern look.
- **Fertile Red** — Highlights fertile areas with red tones.
- **Fertile Green** — Highlights fertile areas with green tones.
- **Red Blue Spectrum** — Uses a spectrum from red to blue to represent elevation or land type.
- **Minerals Yellow** — Emphasizes mineral-rich areas with yellow hues.
- **Fertile Cool** — Applies cool tones to fertile regions for a subdued effect.
- **Fertile Murky** — Uses murky, earthy tones to represent fertile land.

**Land Contours** (`Int32`)  
Controls the visibility and density of contour lines on land areas, highlighting elevation changes.

**Land Contour Intenstity** (`Float2`)  
Adjusts the brightness or opacity of land contour lines, making them more or less prominent.

**Land Bias** (`Single`)  
Applies a bias to the land color or contour mapping, shifting emphasis to certain elevation ranges or features.

**Water Palette** (`Enum`)  
Selects the color palette used for water areas on the map.

- **Classic** — Uses a traditional blue color scheme for water.
- **Black Sea** — Applies a dark, deep blue palette reminiscent of the Black Sea.
- **Caribbean** — Uses bright, turquoise tones inspired by Caribbean waters.

**Water Contours** (`Int32`)  
Controls the visibility and density of contour lines in water areas, indicating depth or underwater features.

**Water Contour Intenstity** (`Float2`)  
Adjusts the brightness or opacity of water contour lines, making them more or less prominent.

**Water Bias** (`Single`)  
Applies a bias to the water color or contour mapping, shifting emphasis to certain depth ranges or features.

**Trim** (`Single`)  
Removes or crops map features outside a specified boundary, focusing the map on a particular region.

**Grid Count** (`Int32`)  
Sets the number of grid divisions overlaid on the map, controlling the density of the grid.

**Grid Intenstity** (`Float2`)  
Adjusts the brightness or opacity of the grid lines, making them more or less visible.

**Grid Style** (`Enum`)  
Specifies where grid lines are drawn on the map.

- **None** — No grid lines are displayed.
- **Over Water** — Grid lines are drawn only over water areas.
- **Over Land** — Grid lines are drawn only over land areas.
- **Everywhere** — Grid lines are drawn over both land and water areas.

**Boundary Thickness** (`Int32`)  
Sets the thickness of boundary lines, such as map borders or region outlines.

**Contour Thickness** (`Int32`)  
Specifies the thickness of contour lines, affecting their visual prominence.

**Grid Thickness** (`Int32`)  
Controls the thickness of grid lines overlaid on the map.
