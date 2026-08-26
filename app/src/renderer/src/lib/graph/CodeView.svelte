<!--
	The graph as the `.massifgraph` file would hold it. The graph itself stays
	the single source of truth: this serializes it, and parses edits straight
	back into it. While the box has focus the typed text is left exactly as
	typed; leaving it prints the graph again, which is where the text gets
	tidied.

	A plain textarea rather than a design-system control: the system has no
	multi-line text field, and `Select`, `Checkbox` and `ListRow` are all
	single-value controls. Type, colour and spacing come from its tokens.
-->
<script lang="ts">
	import { engineClient } from '$lib/engine-client.svelte';
	import { graphStore } from '$lib/graph/graph-store.svelte';
	import type { RegistrySchema } from '$shared/graph-document';
	import { graphComplaints } from '$shared/graph-validation';
	import { parseGraph, serializeGraph } from '$shared/massifgraph';

	let draft = $state('');
	let complaint = $state<string | null>(null);
	let isEditing = $state(false);

	const printed = $derived(serializeGraph(graphStore.project()));

	$effect(() => {
		const text = printed;
		if (isEditing) {
			return;
		}
		draft = text;
		complaint = null;
	});

	function describe(error: unknown): string {
		if (error instanceof Error) {
			return error.message;
		}
		return String(error);
	}

	function apply(text: string): void {
		draft = text;
		const registry = engineClient.registry;
		if (!registry) {
			complaint = 'The engine has not reported its nodes yet';
			return;
		}
		try {
			complaint = applied(text, registry);
		} catch (error) {
			complaint = describe(error);
		}
	}

	/// Nothing changes until the document parses and the registry recognises
	/// every type and port, so a half-typed edit cannot empty the graph.
	function applied(text: string, registry: RegistrySchema): string | null {
		const project = parseGraph(text);
		const complaints = graphComplaints(project.graph, registry);
		if (complaints.length > 0) {
			return complaints.join('\n');
		}
		graphStore.replaceProject(project);
		return null;
	}
</script>

<div class="flex h-full flex-col">
	<textarea
		class="min-h-0 flex-1 resize-none bg-canvas px-4 py-3 font-mono text-2xs leading-relaxed text-default outline-none"
		spellcheck="false"
		autocomplete="off"
		placeholder={'{ "version": 1, "nodes": [], "edges": [] }'}
		value={draft}
		onfocus={() => (isEditing = true)}
		onblur={() => (isEditing = false)}
		oninput={(event) => apply(event.currentTarget.value)}
	></textarea>

	{#if complaint}
		<div class="shrink-0 border-t border-line px-4 py-2 text-2xs whitespace-pre-wrap text-red">
			{complaint}
		</div>
	{/if}
</div>
