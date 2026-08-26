import { describe, expect, test } from 'bun:test';
import type { GraphProject } from './massifgraph';
import { massifGraphFormatVersion, parseGraph, serializeGraph } from './massifgraph';

function sampleProject(): GraphProject {
	return {
		graph: {
			settings: {
				resolution: 1024,
				worldSizeMeters: 20000,
				seed: 1337,
				georeference: {
					coordinateSystem: 'EPSG:31254',
					eastingMinMeters: 39075.5,
					northingMaxMeters: 214292.25
				}
			},
			nodes: [
				{
					id: 'sum',
					typeName: 'Add',
					positionX: 420,
					positionY: 60,
					parameters: {}
				},
				{
					id: 'source',
					typeName: 'ImportDEM',
					positionX: 40,
					positionY: 20,
					parameters: { path: 'oetztal.massifdem', minimumAltitude: 0, maximumAltitude: 3000 }
				},
				{
					id: 'lift',
					typeName: 'Constant',
					positionX: 40,
					positionY: 220,
					parameters: { value: 25 }
				}
			],
			edges: [
				{ fromNodeId: 'lift', fromPort: 0, toNodeId: 'sum', toPort: 1 },
				{ fromNodeId: 'source', fromPort: 0, toNodeId: 'sum', toPort: 0 }
			]
		},
		previewNodeId: 'sum'
	};
}

describe('massifgraph', () => {
	test('a project survives a save and a load', () => {
		const original = sampleProject();
		const reloaded = parseGraph(serializeGraph(original));

		expect(reloaded.previewNodeId).toBe('sum');
		expect(reloaded.graph.settings).toEqual(original.graph.settings);
		expect(reloaded.graph.nodes).toHaveLength(3);
		expect(reloaded.graph.edges).toHaveLength(2);

		const source = reloaded.graph.nodes.find((node) => node.id === 'source');
		expect(source?.typeName).toBe('ImportDEM');
		expect(source?.parameters.path).toBe('oetztal.massifdem');
		expect(source?.positionX).toBe(40);
	});

	test('serialising twice gives the same bytes', () => {
		const project = sampleProject();
		expect(serializeGraph(project)).toBe(serializeGraph(parseGraph(serializeGraph(project))));
	});

	test('node and edge order does not depend on the order they were made', () => {
		const project = sampleProject();
		const shuffled: GraphProject = {
			previewNodeId: project.previewNodeId,
			graph: {
				settings: project.graph.settings,
				nodes: [...project.graph.nodes].reverse(),
				edges: [...project.graph.edges].reverse()
			}
		};

		expect(serializeGraph(shuffled)).toBe(serializeGraph(project));
	});

	test('parameter keys come out sorted, whatever order they went in', () => {
		const project = sampleProject();
		const text = serializeGraph(project);
		const parameterBlock = text.slice(text.indexOf('"maximumAltitude"'));

		expect(parameterBlock.indexOf('"minimumAltitude"')).toBeGreaterThan(0);
		expect(parameterBlock.indexOf('"path"')).toBeGreaterThan(
			parameterBlock.indexOf('"minimumAltitude"')
		);
	});

	test('the file states its format version', () => {
		expect(serializeGraph(sampleProject())).toContain(
			`"formatVersion": ${massifGraphFormatVersion}`
		);
	});

	test('a file from a future format version is refused', () => {
		const text = serializeGraph(sampleProject()).replace('"formatVersion": 1', '"formatVersion": 99');
		expect(() => parseGraph(text)).toThrow('Unsupported .massifgraph format version 99');
	});
});
