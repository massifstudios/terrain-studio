import { describe, expect, test } from 'bun:test';
import type { NodeSchema, PortSchema } from './graph-document';
import { acceptsInputType } from './registry-schema';

function port(name: string, type: PortSchema['type']): PortSchema {
	return { name, description: '', type, fieldSemantics: 'heightfield', optional: false };
}

function schema(inputs: PortSchema[]): NodeSchema {
	return {
		typeName: 'Test',
		category: 'Test',
		description: '',
		version: 1,
		inputs,
		outputs: [],
		parameters: []
	};
}

describe('acceptsInputType', () => {
	test('a node with an input of the type accepts it', () => {
		expect(acceptsInputType(schema([port('In', 'field')]), 'field')).toBe(true);
	});

	test('any input of the type is enough, whatever comes before it', () => {
		const mixed = schema([port('Color', 'color'), port('Mask', 'field')]);
		expect(acceptsInputType(mixed, 'field')).toBe(true);
	});

	test('a node with inputs only of other types refuses', () => {
		expect(acceptsInputType(schema([port('Color', 'color')]), 'field')).toBe(false);
	});

	test('a node with no inputs refuses everything', () => {
		expect(acceptsInputType(schema([]), 'field')).toBe(false);
		expect(acceptsInputType(schema([]), 'points')).toBe(false);
	});
});
