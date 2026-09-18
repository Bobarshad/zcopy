"""Deterministic, wholly SYNTHETIC topology fixtures for local algorithm tests.

Capacities and one-way delays are invented inputs, not measured radio rates,
guarantees, admission grants, or target-hardware evidence. Opposite directed
links explicitly share a physical identity. Resource IDs describe common
bottlenecks; their presence alone does not implement a capacity ledger.
"""

from typing import Callable

from .model import Link, Node, Topology


SYNTHETIC_CAPACITY_BPS = 5_000_000_000
SYNTHETIC_DELAY_MS = 2.0


def _pair(
    links: list[Link],
    source: str,
    target: str,
    physical_id: str,
    *,
    cost: float = 1.0,
    capacity_bps: int = SYNTHETIC_CAPACITY_BPS,
    delay_ms: float = SYNTHETIC_DELAY_MS,
    resource_ids: tuple[str, ...] = (),
    extra_risks: frozenset[str] = frozenset(),
) -> None:
    """Declare two arcs, never infer a reverse radio adjacency."""
    risks = frozenset({f"span:{physical_id}"}) | extra_risks
    for a, b in ((source, target), (target, source)):
        links.append(
            Link(
                id=f"{a}>{b}",
                source=a,
                target=b,
                cost=cost,
                capacity_bps=capacity_bps,
                delay_ms=delay_ms,
                physical_id=physical_id,
                risk_groups=risks,
                resource_ids=resource_ids or (f"capacity:{physical_id}",),
            )
        )


def _grid(prefix: str, nodes: list[Node], links: list[Link]) -> None:
    for row in range(5):
        for col in range(5):
            node_id = f"{prefix}-{row}-{col}"
            nodes.append(Node(node_id, site_id=f"site:{node_id}"))
            for next_row, next_col in ((row + 1, col), (row, col + 1)):
                if next_row < 5 and next_col < 5:
                    neighbor = f"{prefix}-{next_row}-{next_col}"
                    _pair(links, node_id, neighbor, f"rf:{node_id}:{neighbor}")


def _relay_chain() -> Topology:
    nodes = [Node(f"R{i}", role="relay", site_id=f"site:R{i}") for i in range(5)]
    links: list[Link] = []
    for i in range(4):
        _pair(links, f"R{i}", f"R{i + 1}", f"chain:{i}")
    return Topology(nodes, links, name="relay-chain")


def _relay_alternate() -> Topology:
    chain = _relay_chain()
    nodes = list(chain.nodes.values())
    nodes.extend(Node(n, role="relay", site_id=f"site:{n}") for n in ("X1", "X2"))
    links = list(chain.links.values())
    for index, (a, b) in enumerate((("R0", "X1"), ("X1", "X2"), ("X2", "R4"))):
        _pair(links, a, b, f"alternate:{index}", cost=2.0)
    return Topology(nodes, links, name="relay-alternate")


def _grid_5x5() -> Topology:
    nodes: list[Node] = []
    links: list[Link] = []
    _grid("G", nodes, links)
    return Topology(nodes, links, name="grid-5x5")


def _two_grid_nplus1() -> Topology:
    """Two 25-node grids, four explicit relays, and two distinct exit nodes.

    A-0-0 -> GW-A is the default fixed-endpoint request. RA failure disconnects
    GW-A; reaching GW-B through the cross-link is a DIFFERENT exit-service
    choice, not a backup LSP to GW-A or proof of retained Internet TCP/NAT.
    The cross-link's single resource ID is shared by both directed arcs.
    """
    nodes: list[Node] = []
    links: list[Link] = []
    _grid("A", nodes, links)
    _grid("B", nodes, links)
    for node_id in ("RA", "RB", "RDA", "RDB"):
        nodes.append(
            Node(
                node_id,
                role="relay",
                site_id=f"site:{node_id}",
                risk_groups=frozenset({f"power:{node_id}"}),
            )
        )
    nodes.extend(Node(n, role="gateway", site_id=f"site:{n}") for n in ("GW-A", "GW-B"))
    for grid, relay, spare, gateway in (("A", "RA", "RDA", "GW-A"), ("B", "RB", "RDB", "GW-B")):
        _pair(links, f"{grid}-0-4", relay, f"access:{relay}")
        _pair(links, relay, gateway, f"handoff:{grid}", capacity_bps=10_000_000_000)
        _pair(links, f"{grid}-4-4", spare, f"access:{spare}")
    _pair(
        links,
        "RDA",
        "RDB",
        "crosslink:AB",
        cost=3.0,
        capacity_bps=10_000_000_000,
        delay_ms=5.0,
        resource_ids=("shared-crosslink-ab",),
        extra_risks=frozenset({"crosslink-weather-zone"}),
    )
    return Topology(nodes, links, name="two-grid-nplus1")


FIXTURES: dict[str, Callable[[], Topology]] = {
    "relay-chain": _relay_chain,
    "relay-alternate": _relay_alternate,
    "grid-5x5": _grid_5x5,
    "two-grid-nplus1": _two_grid_nplus1,
}

_ENDPOINTS = {
    "relay-chain": ("R0", "R4"),
    "relay-alternate": ("R0", "R4"),
    "grid-5x5": ("G-0-0", "G-4-4"),
    "two-grid-nplus1": ("A-0-0", "GW-A"),
}


def get_fixture(name: str) -> Topology:
    """Return a fresh graph; reject typos without selecting another fixture."""
    try:
        factory = FIXTURES[name]
    except KeyError as exc:
        raise ValueError(f"Unknown fixture {name!r}; choose from {', '.join(sorted(FIXTURES))}") from exc
    return factory()


def fixture_endpoints(name: str) -> tuple[str, str]:
    try:
        return _ENDPOINTS[name]
    except KeyError as exc:
        raise ValueError(f"Unknown fixture {name!r}") from exc
