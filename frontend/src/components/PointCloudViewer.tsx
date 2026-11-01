import { useEffect, useRef, useState } from "react";
import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";
import type { AnsysNode } from "../types/geometry";

interface PointCloudViewerProps {
  nodes: AnsysNode[];
  minTemp: number;
  maxTemp: number;
  onSelect?: (node: AnsysNode | null) => void;
}

// Cyan (cold) -> violet (hot), matching the app's own accent palette
// rather than a generic rainbow colormap.
function temperatureColor(t: number, min: number, max: number): THREE.Color {
  const span = max - min || 1;
  const frac = Math.min(1, Math.max(0, (t - min) / span));
  const cold = new THREE.Color("#38bdf8");
  const hot = new THREE.Color("#f472b6");
  return cold.clone().lerp(hot, frac);
}

export default function PointCloudViewer({ nodes, minTemp, maxTemp, onSelect }: PointCloudViewerProps) {
  const containerRef = useRef<HTMLDivElement>(null);
  const fitRef = useRef<() => void>(() => {});
  const [hoveredId, setHoveredId] = useState<number | null>(null);

  useEffect(() => {
    const container = containerRef.current;
    if (!container) return;

    const width = container.clientWidth;
    const height = container.clientHeight;

    const scene = new THREE.Scene();
    scene.background = new THREE.Color("#0a0e1a");

    const camera = new THREE.PerspectiveCamera(45, width / height, 0.01, 1000);
    const renderer = new THREE.WebGLRenderer({ antialias: true });
    renderer.setSize(width, height);
    renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    container.appendChild(renderer.domElement);

    const positions = new Float32Array(nodes.length * 3);
    const colors = new Float32Array(nodes.length * 3);
    const minX = Math.min(...nodes.map((n) => n.x_m));
    const maxX = Math.max(...nodes.map((n) => n.x_m));
    const minY = Math.min(...nodes.map((n) => n.y_m));
    const maxY = Math.max(...nodes.map((n) => n.y_m));
    const minZ = Math.min(...nodes.map((n) => n.z_m));
    const maxZ = Math.max(...nodes.map((n) => n.z_m));

    nodes.forEach((n, i) => {
      positions[i * 3 + 0] = n.x_m;
      positions[i * 3 + 1] = n.y_m;
      positions[i * 3 + 2] = n.z_m;
      const c = temperatureColor(n.temperature_C, minTemp, maxTemp);
      colors[i * 3 + 0] = c.r;
      colors[i * 3 + 1] = c.g;
      colors[i * 3 + 2] = c.b;
    });

    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute("position", new THREE.BufferAttribute(positions, 3));
    geometry.setAttribute("color", new THREE.BufferAttribute(colors, 3));

    const material = new THREE.PointsMaterial({ size: 0.05, vertexColors: true, sizeAttenuation: true });
    const points = new THREE.Points(geometry, material);
    scene.add(points);

    const gridHelper = new THREE.GridHelper(Math.max(maxX - minX, maxY - minY, 1) * 1.5, 10, 0x1f2937, 0x1f2937);
    scene.add(gridHelper);

    const controls = new OrbitControls(camera, renderer.domElement);
    controls.enableDamping = true;

    const fitToView = () => {
      const center = new THREE.Vector3((minX + maxX) / 2, (minY + maxY) / 2, (minZ + maxZ) / 2);
      const maxDim = Math.max(maxX - minX, maxY - minY, maxZ - minZ, 1e-6);
      const dist = maxDim * 2.2;
      camera.position.set(center.x + dist, center.y + dist * 0.8, center.z + dist);
      camera.near = maxDim / 100;
      camera.far = maxDim * 100;
      camera.updateProjectionMatrix();
      controls.target.copy(center);
      controls.update();
    };
    fitRef.current = fitToView;
    fitToView();

    // Node inspection: raycast against the point cloud on click.
    const raycaster = new THREE.Raycaster();
    raycaster.params.Points = { threshold: Math.max(maxX - minX, 0.1) * 0.03 };
    const pointer = new THREE.Vector2();

    const handleClick = (event: MouseEvent) => {
      const rect = renderer.domElement.getBoundingClientRect();
      pointer.x = ((event.clientX - rect.left) / rect.width) * 2 - 1;
      pointer.y = -((event.clientY - rect.top) / rect.height) * 2 + 1;
      raycaster.setFromCamera(pointer, camera);
      const hits = raycaster.intersectObject(points);
      if (hits.length > 0 && hits[0].index !== undefined) {
        const node = nodes[hits[0].index];
        setHoveredId(node.node_id);
        onSelect?.(node);
      } else {
        setHoveredId(null);
        onSelect?.(null);
      }
    };
    renderer.domElement.addEventListener("click", handleClick);

    let frameId: number;
    const animate = () => {
      frameId = requestAnimationFrame(animate);
      controls.update();
      renderer.render(scene, camera);
    };
    animate();

    const handleResize = () => {
      const w = container.clientWidth;
      const h = container.clientHeight;
      camera.aspect = w / h;
      camera.updateProjectionMatrix();
      renderer.setSize(w, h);
    };
    window.addEventListener("resize", handleResize);

    return () => {
      cancelAnimationFrame(frameId);
      window.removeEventListener("resize", handleResize);
      renderer.domElement.removeEventListener("click", handleClick);
      controls.dispose();
      geometry.dispose();
      material.dispose();
      renderer.dispose();
      container.removeChild(renderer.domElement);
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [nodes, minTemp, maxTemp]);

  return (
    <div className="relative">
      <div ref={containerRef} className="w-full h-[380px] rounded-lg overflow-hidden" />
      <button
        onClick={() => fitRef.current()}
        className="absolute top-2 right-2 px-2.5 py-1 text-xs rounded border border-nexus-border bg-nexus-panel/90 text-nexus-muted hover:text-white hover:border-nexus-accent"
      >
        Fit to view
      </button>
      {hoveredId !== null && (
        <div className="absolute bottom-2 left-2 px-2.5 py-1 text-xs rounded border border-nexus-border bg-nexus-panel/90 text-nexus-muted">
          Selected node #{hoveredId} — see table below
        </div>
      )}
    </div>
  );
}
