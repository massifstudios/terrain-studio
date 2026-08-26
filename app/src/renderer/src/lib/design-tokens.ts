/**
 * Reads a design-system colour token so the WebGL canvases clear to the same
 * surface as the chrome around them. Components never hold colour literals.
 */
export function readColorToken(name: string): string {
	return getComputedStyle(document.documentElement).getPropertyValue(name).trim();
}
