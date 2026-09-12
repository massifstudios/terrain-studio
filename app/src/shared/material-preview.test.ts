import { describe, expect, test } from 'bun:test';
import type { NodeSchema, ParameterSchema, PortSchema } from './graph-document';
import { materialLayers } from './material-preview';

function maskPort(name: string): PortSchema {
	return { name, description: '', type: 'field', fieldSemantics: 'mask', optional: false };
}

function heightPort(name: string): PortSchema {
	return { name, description: '', type: 'field', fieldSemantics: 'heightfield', optional: false };
}

function textureParameter(name: string, textures: string[]): ParameterSchema {
	return {
		name,
		label: name,
		description: '',
		unit: '',
		type: 'texture',
		defaultValue: 0,
		minimum: 0,
		maximum: textures.length - 1,
		defaultText: '',
		options: textures.map((texture, value) => ({ name: texture, label: texture, value }))
	};
}

function floatParameter(name: string): ParameterSchema {
	return { ...textureParameter(name, []), type: 'float', options: [] };
}

function schema(outputs: PortSchema[], parameters: ParameterSchema[]): NodeSchema {
	return {
		typeName: 'Test',
		category: 'Test',
		description: '',
		version: 1,
		inputs: [],
		outputs,
		parameters
	};
}

const textures = ['rock.jpg', 'grass.jpg', 'snow.jpg'];

describe('materialLayers', () => {
	test('pairs texture parameters with mask outputs in order', () => {
		const node = schema(
			[maskPort('Base'), maskPort('Layer 2')],
			[textureParameter('baseTexture', textures), textureParameter('layer2Texture', textures)]
		);
		expect(materialLayers(node, { baseTexture: 2, layer2Texture: 1 })).toEqual([
			{ outputPort: 0, textureName: 'snow.jpg' },
			{ outputPort: 1, textureName: 'grass.jpg' }
		]);
	});

	test('skips outputs that are not masks when counting ports', () => {
		const node = schema(
			[heightPort('Out'), maskPort('Snow')],
			[textureParameter('snowTexture', textures)]
		);
		expect(materialLayers(node, { snowTexture: 0 })).toEqual([
			{ outputPort: 1, textureName: 'rock.jpg' }
		]);
	});

	test('a node without texture parameters has no layers', () => {
		const node = schema([maskPort('Out')], [floatParameter('radius')]);
		expect(materialLayers(node, { radius: 3 })).toEqual([]);
	});

	test('a missing or stale value falls back to the first texture', () => {
		const node = schema([maskPort('Base')], [textureParameter('baseTexture', textures)]);
		expect(materialLayers(node, {})).toEqual([{ outputPort: 0, textureName: 'rock.jpg' }]);
		expect(materialLayers(node, { baseTexture: 99 })).toEqual([
			{ outputPort: 0, textureName: 'rock.jpg' }
		]);
	});

	test('more texture parameters than masks pair only as far as the masks go', () => {
		const node = schema(
			[maskPort('Base')],
			[textureParameter('baseTexture', textures), textureParameter('layer2Texture', textures)]
		);
		expect(materialLayers(node, { baseTexture: 1, layer2Texture: 2 })).toHaveLength(1);
	});

	test('no more than four layers are planned', () => {
		const outputs = ['a', 'b', 'c', 'd', 'e'].map(maskPort);
		const parameters = ['a', 'b', 'c', 'd', 'e'].map((name) => textureParameter(name, textures));
		expect(materialLayers(schema(outputs, parameters), {})).toHaveLength(4);
	});
});
