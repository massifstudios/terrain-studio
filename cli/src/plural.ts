/// Reports read badly with "1 nodes" in them, and every count printed by a
/// command goes through here rather than each one deciding for itself.
export function countOf(count: number, noun: string): string {
	if (count === 1) {
		return `1 ${noun}`;
	}
	return `${count} ${noun}s`;
}
