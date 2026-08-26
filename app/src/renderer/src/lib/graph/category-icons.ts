/**
 * The icon a node is drawn with.
 *
 * Icons key off the category the registry reports, never off the type name: a
 * node registered tomorrow, or one arriving from a plugin, is drawn without
 * anything here changing. A category nobody thought of falls back rather than
 * leaving a hole.
 */

import type { IconComponent } from '@neoworks-dev/ui';
import ArrowsMergeIcon from 'phosphor-svelte/lib/ArrowsMergeIcon';
import CubeIcon from 'phosphor-svelte/lib/CubeIcon';
import DropIcon from 'phosphor-svelte/lib/DropIcon';
import ExportIcon from 'phosphor-svelte/lib/ExportIcon';
import FunnelIcon from 'phosphor-svelte/lib/FunnelIcon';
import MountainsIcon from 'phosphor-svelte/lib/MountainsIcon';
import PaletteIcon from 'phosphor-svelte/lib/PaletteIcon';
import SelectionIcon from 'phosphor-svelte/lib/SelectionIcon';
import SignInIcon from 'phosphor-svelte/lib/SignInIcon';
import TreeIcon from 'phosphor-svelte/lib/TreeIcon';

const categoryIcons: Record<string, IconComponent> = {
	Combine: ArrowsMergeIcon,
	Erode: DropIcon,
	Filter: FunnelIcon,
	Generate: MountainsIcon,
	Input: SignInIcon,
	Mask: SelectionIcon,
	Output: ExportIcon,
	Texture: PaletteIcon,
	Vegetation: TreeIcon
};

export function iconForCategory(category: string): IconComponent {
	if (category in categoryIcons) {
		return categoryIcons[category];
	}
	return CubeIcon;
}
