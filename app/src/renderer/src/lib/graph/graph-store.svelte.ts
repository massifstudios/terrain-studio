import type {
	GraphDocument,
	GraphEdgeDocument,
	GraphNodeDocument,
	GraphSettings,
	NodeSchema,
	ParameterValue,
	RegistrySchema
} from '$shared/graph-document';
import { emptyGraphSettings } from '$shared/graph-document';
import {
	canConnect,
	copiedSubgraph,
	findNode,
	nextNodeId,
	withConnection,
	withMovedNodes,
	withoutInput,
	withoutNodes,
	withParameter,
	withPastedSubgraph,
	type ConnectionRequest,
	type GraphContents
} from '$shared/graph-operations';
import type { GraphProject } from '$shared/massifgraph';
import { defaultParameters } from '$shared/registry-schema';

/// A preview node named by a graph that no longer holds it is no preview at all.
function survivingPreviewNodeId(project: GraphProject, surviving: Set<string>): string | null {
	if (project.previewNodeId === null || !surviving.has(project.previewNodeId)) {
		return null;
	}
	return project.previewNodeId;
}

/**
 * The single source of truth for the graph. The node editor renders this and
 * emits mutations; no editor component holds graph state of its own. Every
 * rule lives in `$shared/graph-operations`, which this only makes reactive.
 */
class GraphStore {
	settings = $state<GraphSettings>(emptyGraphSettings());
	nodes = $state<GraphNodeDocument[]>([]);
	edges = $state<GraphEdgeDocument[]>([]);
	selectedNodeIds = $state<string[]>([]);
	previewNodeId = $state<string | null>(null);
	filePath = $state<string | null>(null);
	hasUnsavedChanges = $state(false);
	/// A session preference, not part of the project: what a Build would use.
	buildResolution = $state(8192);

	document(): GraphDocument {
		return {
			settings: $state.snapshot(this.settings),
			nodes: $state.snapshot(this.nodes),
			edges: $state.snapshot(this.edges)
		};
	}

	project(): GraphProject {
		return { graph: this.document(), previewNodeId: this.previewNodeId };
	}

	findNode(nodeId: string): GraphNodeDocument | null {
		return findNode(this.nodes, nodeId);
	}

	get selectedNode(): GraphNodeDocument | null {
		if (this.selectedNodeIds.length !== 1) {
			return null;
		}
		return this.findNode(this.selectedNodeIds[0]);
	}

	addNode(schema: NodeSchema, positionX: number, positionY: number): string {
		const id = nextNodeId(this.nodes, schema.typeName);
		this.nodes = [
			...this.nodes,
			{ id, typeName: schema.typeName, positionX, positionY, parameters: defaultParameters(schema) }
		];
		this.select([id]);
		this.#touch();
		return id;
	}

	/// What the last copy took, held for the session. It is a graph fragment
	/// rather than text, so a paste is exact and needs no parsing.
	#clipboard: GraphContents | null = null;

	get hasClipboard(): boolean {
		return this.#clipboard !== null;
	}

	copySelection(): void {
		if (this.selectedNodeIds.length === 0) {
			return;
		}
		this.#clipboard = copiedSubgraph(
			{ nodes: $state.snapshot(this.nodes), edges: $state.snapshot(this.edges) },
			this.selectedNodeIds
		);
	}

	/// The copied nodes come in moved by the offset and become the selection.
	paste(deltaX: number, deltaY: number): void {
		if (this.#clipboard === null) {
			return;
		}
		const pasted = withPastedSubgraph(
			{ nodes: this.nodes, edges: this.edges },
			this.#clipboard,
			deltaX,
			deltaY
		);
		this.nodes = pasted.contents.nodes;
		this.edges = pasted.contents.edges;
		this.select(pasted.pastedNodeIds);
		this.#touch();
	}

	/// Where the copied nodes sit, so a paste can be placed relative to them.
	clipboardOrigin(): { x: number; y: number } | null {
		if (this.#clipboard === null || this.#clipboard.nodes.length === 0) {
			return null;
		}
		return {
			x: Math.min(...this.#clipboard.nodes.map((node) => node.positionX)),
			y: Math.min(...this.#clipboard.nodes.map((node) => node.positionY))
		};
	}

	moveNodes(nodeIds: string[], deltaX: number, deltaY: number): void {
		this.nodes = withMovedNodes(this.nodes, nodeIds, deltaX, deltaY);
		this.#touch();
	}

	removeNodes(nodeIds: string[]): void {
		const remaining = withoutNodes({ nodes: this.nodes, edges: this.edges }, nodeIds);
		this.nodes = remaining.nodes;
		this.edges = remaining.edges;

		const removing = new Set(nodeIds);
		this.selectedNodeIds = this.selectedNodeIds.filter((id) => !removing.has(id));
		if (this.previewNodeId !== null && removing.has(this.previewNodeId)) {
			this.previewNodeId = null;
		}
		this.#touch();
	}

	canConnect(registry: RegistrySchema | null, request: ConnectionRequest): boolean {
		return canConnect(registry, this.nodes, this.edges, request);
	}

	connect(request: ConnectionRequest): void {
		this.edges = withConnection(this.edges, request);
		this.#touch();
	}

	disconnectInput(toNodeId: string, toPort: number): void {
		this.edges = withoutInput(this.edges, toNodeId, toPort);
		this.#touch();
	}

	setParameter(nodeId: string, name: string, value: ParameterValue): void {
		this.nodes = withParameter(this.nodes, nodeId, name, value);
		this.#touch();
	}

	/// The whole project at once, as the code view hands it over: the text holds
	/// the settings and the preview too, so an edit to either takes effect. The
	/// file it was loaded from does not change, and neither does its being
	/// unsaved.
	replaceProject(project: GraphProject): void {
		const surviving = new Set(project.graph.nodes.map((node) => node.id));
		this.settings = project.graph.settings;
		this.nodes = project.graph.nodes;
		this.edges = project.graph.edges;
		this.selectedNodeIds = this.selectedNodeIds.filter((id) => surviving.has(id));
		this.previewNodeId = survivingPreviewNodeId(project, surviving);
		this.#touch();
	}

	select(nodeIds: string[]): void {
		this.selectedNodeIds = nodeIds;
		if (nodeIds.length === 1) {
			this.previewNodeId = nodeIds[0];
		}
	}

	load(project: GraphProject, filePath: string | null): void {
		this.settings = project.graph.settings;
		this.nodes = project.graph.nodes;
		this.edges = project.graph.edges;
		this.previewNodeId = project.previewNodeId;
		this.selectedNodeIds = [];
		this.filePath = filePath;
		this.hasUnsavedChanges = false;
	}

	markSaved(filePath: string): void {
		this.filePath = filePath;
		this.hasUnsavedChanges = false;
	}

	#touch(): void {
		this.hasUnsavedChanges = true;
	}
}

export const graphStore = new GraphStore();
