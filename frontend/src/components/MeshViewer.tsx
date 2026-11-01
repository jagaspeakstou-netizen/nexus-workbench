import { useEffect, useRef } from "react";
import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";
import type { StlTriangle } from "../types/geometry";

interface MeshViewerProps {
  triangles: StlTriangle[];
  boundingBox: { min: [number, number, number]; max: [number, number, number] };
}

// Real Three.js scene (not a screenshot/placeholder): builds a
// BufferGeometry directly from the parsed triangle list, with
// OrbitControls for rotate/pan/zoom and a "fit to view" button that
// recenters the camera on the mesh's actual bounding box.
export default function MeshViewer({ triangles, boundingBox }: MeshViewerProps) {
  const containerRef = useRef<HTMLDivElement>(null);
  const fitRef = useRef<() => void>(() => {});

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

    // Geometry from the parsed C++ triangle list -- non-indexed, 3
    // vertices per triangle, exactly matching what the backend sent.
    const positions = new Float32Array(triangles.length * 9);
    triangles.forEach((t, i) => {
      const o = i * 9;
      positions[o + 0] = t.v0[0]; positions[o + 1] = t.v0[1]; positions[o + 2] = t.v0[2];
      positions[o + 3] = t.v1[0]; positions[o + 4] = t.v1[1]; positions[o + 5] = t.v1[2];
      positions[o + 6] = t.v2[0]; positions[o + 7] = t.v2[1]; positions[o + 8] = t.v2[2];
    });
    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute("position", new THREE.BufferAttribute(positions, 3));
    geometry.computeVertexNormals();

    const material = new THREE.MeshStandardMaterial({
      color: "#38bdf8",
      metalness: 0.1,
      roughness: 0.6,
      side: THREE.DoubleSide,
      flatShading: true,
    });
    const mesh = new THREE.Mesh(geometry, material);
    scene.add(mesh);

    const wireframe = new THREE.LineSegments(
      new THREE.WireframeGeometry(geometry),
      new THREE.LineBasicMaterial({ color: "#818cf8", transparent: true, opacity: 0.25 })
    );
    scene.add(wireframe);

    scene.add(new THREE.AmbientLight(0xffffff, 0.5));
    const dirLight = new THREE.DirectionalLight(0xffffff, 1.0);
    dirLight.position.set(5, 8, 6);
    scene.add(dirLight);

    const controls = new OrbitControls(camera, renderer.domElement);
    controls.enableDamping = true;

    const fitToView = () => {
      const [minX, minY, minZ] = boundingBox.min;
      const [maxX, maxY, maxZ] = boundingBox.max;
      const center = new THREE.Vector3((minX + maxX) / 2, (minY + maxY) / 2, (minZ + maxZ) / 2);
      const size = new THREE.Vector3(maxX - minX, maxY - minY, maxZ - minZ);
      const maxDim = Math.max(size.x, size.y, size.z, 1e-6);
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
      controls.dispose();
      geometry.dispose();
      material.dispose();
      renderer.dispose();
      container.removeChild(renderer.domElement);
    };
  }, [triangles, boundingBox]);

  return (
    <div className="relative">
      <div ref={containerRef} className="w-full h-[380px] rounded-lg overflow-hidden" />
      <button
        onClick={() => fitRef.current()}
        className="absolute top-2 right-2 px-2.5 py-1 text-xs rounded border border-nexus-border bg-nexus-panel/90 text-nexus-muted hover:text-white hover:border-nexus-accent"
      >
        Fit to view
      </button>
    </div>
  );
}
