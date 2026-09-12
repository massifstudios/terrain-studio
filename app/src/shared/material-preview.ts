/**
 * How the viewport lays textures over a node: its texture parameters are
 * paired with its mask outputs in declaration order, so the first texture is
 * tiled where the first mask has weight, the second where the second has, and
 * so on. Nothing here is terrain math — the weights come from the engine and
 * the textures are only named.
 */

import type { NodeSchema, ParameterSchema, ParameterValue } from './graph-document';

/// The shader blends this many layers in one pass; further ones are not drawn.
export const materialLayerLimit = 4;

export interface MaterialLayer {
	outputPort: number;
	textureName: string;
}

function maskOutputPorts(schema: NodeSchema): number[] {
	const ports: number[] = [];
	for (let port = 0; port < schema.outputs.length; port += 1) {
		const output = schema.outputs[port];
		if (output.type === 'field' && output.fieldSemantics === 'mask') {
			ports.push(port);
		}
	}
	return ports;
}

/// A value naming no option falls back to the first, which is also what a
/// project saved before the parameter existed gets.
function textureNameFor(parameter: ParameterSchema, value: ParameterValue | undefined): string | null {
	const chosen = parameter.options.find((option) => option.value === Number(value));
	if (chosen) {
		return chosen.name;
	}
	const first = parameter.options[0];
	if (!first) {
		return null;
	}
	return first.name;
}

export function materialLayers(
	schema: NodeSchema,
	parameters: Record<string, ParameterValue>
): MaterialLayer[] {
	const textureParameters = schema.parameters.filter((parameter) => parameter.type === 'texture');
	const maskPorts = maskOutputPorts(schema);
	const count = Math.min(textureParameters.length, maskPorts.length, materialLayerLimit);
	const layers: MaterialLayer[] = [];
	for (let index = 0; index < count; index += 1) {
		const parameter = textureParameters[index];
		const textureName = textureNameFor(parameter, parameters[parameter.name]);
		if (textureName === null) {
			return [];
		}
		layers.push({ outputPort: maskPorts[index], textureName });
	}
	return layers;
}
