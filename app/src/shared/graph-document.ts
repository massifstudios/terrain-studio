/**
 * The graph as the UI holds it and as `.massifgraph` stores it. Node types are
 * never named here: everything the UI knows about a node comes from the
 * registry the engine reports.
 */

export type ParameterValue = number | string | boolean;

export interface GraphGeoReference {
	coordinateSystem: string;
	eastingMinMeters: number;
	northingMaxMeters: number;
}

export interface GraphSettings {
	resolution: number;
	worldSizeMeters: number;
	seed: number;
	georeference: GraphGeoReference;
}

export interface GraphNodeDocument {
	id: string;
	typeName: string;
	positionX: number;
	positionY: number;
	parameters: Record<string, ParameterValue>;
}

export interface GraphEdgeDocument {
	fromNodeId: string;
	fromPort: number;
	toNodeId: string;
	toPort: number;
}

export interface GraphDocument {
	settings: GraphSettings;
	nodes: GraphNodeDocument[];
	edges: GraphEdgeDocument[];
}

/* ---------------------------------------------------------- the registry -- */

export type PortTypeName = 'field' | 'color' | 'points';
export type FieldSemanticsName = 'heightfield' | 'mask';
export type ParameterTypeName =
	| 'float'
	| 'int'
	| 'bool'
	| 'enum'
	| 'seed'
	| 'colorRamp'
	| 'path'
	| 'text';

export interface PortSchema {
	name: string;
	description: string;
	type: PortTypeName;
	fieldSemantics: FieldSemanticsName;
	optional: boolean;
}

export interface EnumOptionSchema {
	name: string;
	label: string;
	value: number;
}

export interface ParameterSchema {
	name: string;
	label: string;
	description: string;
	unit: string;
	type: ParameterTypeName;
	defaultValue: number;
	minimum: number;
	maximum: number;
	defaultText: string;
	options: EnumOptionSchema[];
}

export interface NodeSchema {
	typeName: string;
	category: string;
	description: string;
	version: number;
	inputs: PortSchema[];
	outputs: PortSchema[];
	parameters: ParameterSchema[];
}

export interface RegistrySchema {
	abiVersion: number;
	nodes: NodeSchema[];
}
