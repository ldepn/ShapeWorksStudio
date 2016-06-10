#include "stdafx.h"
#include "FEFillHole.h"
#include "FEAutoMesher.h"

//-----------------------------------------------------------------------------
// Create a new mesh where the hole is filled. The hole is defined by a node
// that lies on the edge of the hole.
FEMesh* FEFillHole::Apply(FEMesh* pm)
{
	// find a selected node
	int inode = -1;
	for (int i=0; i<pm->Nodes(); i++) { if (pm->Node(i).IsSelected()) { inode = i; break; }}
	if (inode == -1) return 0;

	// find the ring that this node belongs to
	// where a ring is a closed loop of ordered edges
	EdgeRing ring;
	if (FindEdgeRing(*pm, inode, ring) == false) return 0;
	if (ring.empty()) return 0;

	// divide the rings using recursive method
	vector<FACE> tri_list;
	if (optimize)
	{
		if (DivideRing(ring, tri_list) == false) return 0;
	}
	else
	{
		if (DivideRing1(ring, tri_list) == false) return 0;
	}

	// see how many new faces we have
	int new_faces = (int) tri_list.size();

	// create a copy of the original mesh
	FEMesh* pnew = new FEMesh(*pm);

	// allocate room for the new faces
	int NE = pnew->Elements();
	pnew->Create(0, NE + new_faces);

	// insert the new triangles into the mesh
	for (int i=0; i<new_faces; ++i)
	{
		FEElement& el = pnew->Element(i + NE);
		FACE& fi = tri_list[i];
		el.SetType(FE_TRI3);
		el.m_node[0] = fi.n[0];
		el.m_node[1] = fi.n[1];
		el.m_node[2] = fi.n[2];
		el.m_gid = 0;
	}

	// next, we use the auto-mesher to reconstruct all faces, edges and nodes
	FEAutoMesher mesher;
	mesher.BuildMesh(pnew);

	// done
	return pnew;
}

//-----------------------------------------------------------------------------
// fill all holes
void FEFillHole::FillAllHoles(FEMesh* pm)
{
	for (int i=0; i<pm->Nodes(); ++i) pm->Node(i).m_ntag = 0;

	for (int i=0; i<pm->Edges(); ++i)
	{
		FEEdge& ed = pm->Edge(i);
		pm->Node(ed.n[0]).m_ntag = 1;
		pm->Node(ed.n[1]).m_ntag = 1;
	}

	vector<EdgeRing> ring;
	int inode = -1;
	do
	{
		inode = -1;
		for (int i=0; i<pm->Nodes(); ++i)
		{
			if (pm->Node(i).m_ntag == 1)
			{
				inode = i;
				break;
			}
		}

		if (inode >= 0)
		{
			EdgeRing ri; 
			if (FindEdgeRing(*pm, inode, ri)) ring.push_back(ri);
			for (int i=0; i<ri.size(); ++i) pm->Node(ri[i]).m_ntag = 0;
		}
	}
	while (inode != -1);

	vector<FACE> tri_list;
	for (int i=0; i<ring.size(); ++i)
	{
		vector<FACE> tri;
		DivideRing(ring[i], tri);
		tri_list.insert(tri_list.end(), tri.begin(), tri.end());
	}

	// see how many new faces we have
	int new_faces = (int) tri_list.size();

	// allocate room for the new faces
	int NE = pm->Elements();
	pm->Create(0, NE + new_faces);

	// insert the new triangles into the mesh
	for (int i=0; i<new_faces; ++i)
	{
		FEElement& el = pm->Element(i + NE);
		FACE& fi = tri_list[i];
		el.SetType(FE_TRI3);
		el.m_node[0] = fi.n[0];
		el.m_node[1] = fi.n[1];
		el.m_node[2] = fi.n[2];
		el.m_gid = 0;
	}

	// next, we use the auto-mesher to reconstruct all faces, edges and nodes
	FEAutoMesher mesher;
	mesher.BuildMesh(pm);
}

//-----------------------------------------------------------------------------
bool FEFillHole::FindEdgeRing(FEMesh& mesh, int inode, FEFillHole::EdgeRing& ring)
{
	// let's make sure the ring is empty
	ring.clear();

	// find an edge that this node belongs to
	int iedge = -1;
	for (int i=0; i<mesh.Edges(); ++i)
	{
		FEEdge& edge = mesh.Edge(i);
		if ((edge.n[0] == inode) || (edge.n[1] == inode))
		{
			iedge = i;
			break;
		}
	}
	if (iedge == -1) return false;

	// the tricky part is that we need to figure out the winding of the ring
	// so that we can create the faces with the correct orientation. The way
	// we do this is by first identifying a face that has this edge and then
	// see what the winding is of this face
	int iface = -1;
	FEEdge& edge = mesh.Edge(iedge);
	ring.m_winding = 0;
	for (int i=0; i<mesh.Elements(); ++i)
	{
		FEElement& el = mesh.Element(i);
		for (int j=0; j<3; ++j)
		{
			int jn = (j+1)%3;
			if ((el.m_node[j] == edge.n[0]) && (el.m_node[jn] == edge.n[1]))
			{
				// if we get here the edge is in the same direction
				ring.m_winding = -1;
				break;
			}
			else if ((el.m_node[j] == edge.n[1]) && (el.m_node[jn] == edge.n[0]))
			{
				// if we get here the edge is in the opposite winding
				ring.m_winding = 1;
				break;
			}
		}

		// we stop if we figured out the winding
		if (ring.m_winding != 0) break;
	}

	// add the initial node to the ring as a starting point
	int jnode = inode;
	ring.add(jnode, mesh.Node(jnode).r);
	
	// see in which direction we will be looping and if we need to flip the winding
	if (jnode == edge.n[1]) ring.m_winding *= -1;

	// now, loop over all the edges of the ring and add the nodes
	do
	{
		FEEdge& edge = mesh.Edge(iedge);
		if (edge.n[0] == jnode)
		{ 
			jnode = edge.n[1]; iedge = edge.m_nbr[1];
		}
		else 
		{
			jnode = edge.n[0]; iedge = edge.m_nbr[0];
		}

		// if we've reached an open-ended edge we have a problem.
		if (iedge == -1) return false;

		// add the node (unless we're back we're we started)
		if (jnode != inode) ring.add(jnode, mesh.Node(jnode).r);
	}
	while (jnode != inode);

	return true;
}

//------------------------------------------------------------------------------
bool FEFillHole::DivideRing(EdgeRing& ring, vector<FACE>& tri_list)
{
	// make sure this ring has at least three nodes
	assert(ring.size() >= 3);
	if (ring.size() < 3) return false;

	// if the ring has only three nodes, we define a new face
	if (ring.size() == 3)
	{
		FACE f;
		if (ring.m_winding == 1)
		{
			f.n[0] = ring[0]; f.r[0] = ring.m_r[0];
			f.n[1] = ring[1]; f.r[1] = ring.m_r[1];
			f.n[2] = ring[2]; f.r[2] = ring.m_r[2];
		}
		else
		{
			f.n[0] = ring[2]; f.r[0] = ring.m_r[2];
			f.n[1] = ring[1]; f.r[1] = ring.m_r[1];
			f.n[2] = ring[0]; f.r[2] = ring.m_r[0];
		}
		tri_list.push_back(f);
		return true;
	}

	// get the normal to this ring
	vec3d t = RingNormal(ring);

	if (ring.size() > 10)
	{
		// if the ring is too large, finding the optimal number of iterations is too challenging,
		// so we use a quick and dirty method. 
		// We pick a node, and find the node that is farthest away. If those two nodes split the 
		// ring in a valid split, then we divide and conquer. If not, we pick the next node and repeat.
		const int N = (const int) ring.size();
		for (int i=0; i<N; ++i)
		{
			int ni = (i+N/2)%N;
			vec3d ri = ring.m_r[ni];
			double Dmax = 0;
			int jmax = 0;
			for (int j=0; j<N; ++j)
			{
				vec3d rj = ring.m_r[j];
				double D = (ri - rj)*(ri - rj);
				if (D > Dmax)
				{
					Dmax = D;
					jmax = j;
				}
			}

			// get the dividing plane normal
			vec3d rj = ring.m_r[jmax];
			vec3d d = ri - rj;
			vec3d pn = d ^ t;

			// get the left and right ears
			EdgeRing left, right;
			ring.GetLeftEar (ni, jmax, left);
			ring.GetRightEar(ni, jmax, right);

			// make sure we actually cut off an ear
			assert(left.size() > 2);
			assert(right.size() > 2);

			// see if this split is valid
			if (IsValidSplit(left, right, ri, pn))
			{
				vector<FACE> tri_left, tri_right;
				bool bret1 = DivideRing(left , tri_left);
				bool bret2 = DivideRing(right, tri_right);
				if (bret1 && bret2)
				{
					// merge the lists
					tri_list.insert(tri_list.end(), tri_left.begin(), tri_left.end());
					tri_list.insert(tri_list.end(), tri_right.begin(), tri_right.end());
				}

				break;
			}
		}
	}
	else
	{
		// determine the optimal triangulation
		vector<FACE> tri_optimal;	// the optimal triangulation
		vector<FACE> tri_current;	// the current triangulation

		EdgeRing left, right;

		// if we get here, try to divide the ring in two rings and apply the division recursively
		double amax = 0.0;
		int N = ring.size();
		int N1 = (N%2==0?N/2:N/2+1);
		for (int i=0; i<N1; ++i)
		{
			for (int j=2; j<N-1; ++j)
			{
				// clear the current triangulation
				tri_current.clear();

				// get the dividing plane normal
				vec3d ri = ring.m_r[i      ];
				vec3d rj = ring.m_r[(i+j)%N];
				vec3d d = ri - rj;
				vec3d pn = d ^ t;

				// get the left and right ears
				ring.GetLeftEar (i, (i+j)%N, left);
				ring.GetRightEar(i, (i+j)%N, right);

				// make sure we actually cut off an ear
				assert(left.size() > 2);
				assert(right.size() > 2);

				// see if this split is valid
				if (IsValidSplit(left, right, ri, pn))
				{
					vector<FACE> tri_left, tri_right;
					bool bret1 = DivideRing(left , tri_left);
					bool bret2 = DivideRing(right, tri_right);
					if (bret1 && bret2)
					{
						// merge the lists
						tri_current.insert(tri_current.end(), tri_left.begin(), tri_left.end());
						tri_current.insert(tri_current.end(), tri_right.begin(), tri_right.end());

						// get the minimum quality
	//					double amin = min_tri_area(tri_current);
						double amin = min_tri_quality(tri_current);

						// get the minimum quality
						if (amin > amax) 
						{
							tri_optimal = tri_current;
							amax = amin;
						}
					}
				}
			}
		}

		// store the optimal triangulation
		tri_list = tri_optimal;
	}

	return (tri_list.empty() == false);
}



//------------------------------------------------------------------------------
bool FEFillHole::DivideRing1(EdgeRing& ring, vector<FACE>& tri_list)
{
	// make sure this ring has at least three nodes
	assert(ring.size() >= 3);
	if (ring.size() < 3) return false;

	// if the ring has only three nodes, we define a new face
	if (ring.size() == 3)
	{
		FACE f;
		if (ring.m_winding == 1)
		{
			f.n[0] = ring[0]; f.r[0] = ring.m_r[0];
			f.n[1] = ring[1]; f.r[1] = ring.m_r[1];
			f.n[2] = ring[2]; f.r[2] = ring.m_r[2];
		}
		else
		{
			f.n[0] = ring[2]; f.r[0] = ring.m_r[2];
			f.n[1] = ring[1]; f.r[1] = ring.m_r[1];
			f.n[2] = ring[0]; f.r[2] = ring.m_r[0];
		}
		tri_list.push_back(f);
		return true;
	}

	// get the normal to this ring
	vec3d t = RingNormal(ring);

	vector<FACE> tri_optimal;	// the optimal triangulation
	vector<FACE> tri_current;	// the current triangulation

	// if we get here, try to divide the ring in two rings and apply the division recursively
	double amax = 0.0;
	int N = ring.size();
	int N1 = (N%2==0?N/2:N/2+1);
	EdgeRing left, right;
	for (int i=0; i<N1; ++i)
	{
		for (int j=N/2; j<N-1; ++j)
		{
			if (tri_optimal.size() == N-2)
				break;
			// clear the current triangulation
			tri_current.clear();

			// get the dividing plane normal
			vec3d ri = ring.m_r[i      ];
			vec3d rj = ring.m_r[(i+j)%N];
			vec3d d = ri - rj;
			vec3d pn = d ^ t;

			// get the left and right ears
			ring.GetLeftEar (i, (i+j)%N, left);
			ring.GetRightEar(i, (i+j)%N, right);

			// make sure we actually cut off an ear
			assert(left.size() > 2);
			assert(right.size() > 2);

			// see if this split is valid
			if (IsValidSplit(left, right, ri, pn))
			{
				vector<FACE> tri_left, tri_right;
				bool bret1 = DivideRing1(left , tri_left);
				bool bret2 = DivideRing1(right, tri_right);
				if (bret1 && bret2)
				{
					// merge the lists
					tri_current.insert(tri_current.end(), tri_left.begin(), tri_left.end());
					tri_current.insert(tri_current.end(), tri_right.begin(), tri_right.end());

					// get the minimum quality
//					double amin = min_tri_area(tri_current);
					double amin = min_tri_quality(tri_current);

					// get the minimum quality
					if (amin > amax) 
					{
						tri_optimal = tri_current;
						amax = amin;
					}
				}
			}
		}
	}

	// store the optimal triangulation
	tri_list = tri_optimal;
	return (tri_list.empty() == false);
}


//-----------------------------------------------------------------------------
// Calculate the normal of the plane that approximate passes through the ring
vec3d FEFillHole::RingNormal(FEFillHole::EdgeRing& ring)
{
	int N = (int) ring.size();
	std::vector<vec3d>& r = ring.m_r;

	// find the plane of the ring
	// p = point on plane (here center of ring)
	vec3d p(r[0]);
	for (int i=1; i<N; ++i) p += r[i];
	p /= (double) N;

	// get the (approximate) plane normal
	vec3d t(0,0,0);
	for (int i=0; i<N-1; ++i) t += (p - r[i]) ^ (p - r[i+1]);
	t.Normalize();

	return t;
}

//-----------------------------------------------------------------------------
// Determine if this is a valid split. The split is valid if the ears lie on opposite
// side of the plane defined by (p,t)
bool FEFillHole::IsValidSplit(FEFillHole::EdgeRing& left, FEFillHole::EdgeRing& right, const vec3d& p, const vec3d& t)
{
	int n0 = GetPlaneOrientation(left , p, t);
	int n1 = GetPlaneOrientation(right, p, t);
	
	// if the ears are on either side of the plane, the product of the signs should be -1
	if (n0 * n1 < 0) return true;
	else return false;
}

//-----------------------------------------------------------------------------
// Figure out which side of the plane the ring lies. Returns +1 of the ring lies
// on the positive side, -1 if it lies on the negative side and 0 if it lies on boths sides
int FEFillHole::GetPlaneOrientation(FEFillHole::EdgeRing& ring, const vec3d& p, const vec3d& t)
{
	// loop over all the points in the ring, except the first and last
	// (we skip the first and last since they should be on the plane by design)
	int N = (int) ring.size();
	int nsign = 0;
	for (int i=1; i<N-1; ++i)
	{
		vec3d& ri = ring.m_r[i];
		double d = t*(ri - p);
		if (i==1) nsign = (d > 0 ? 1 : -1);
		else
		{
			int si = (d > 0 ? 1 : -1);
			if (si != nsign) return 0;
		}
	}
	return nsign;
}

//-----------------------------------------------------------------------------
// this calculates the ratio of the shortest diagonal to the longest edge
double FEFillHole::tri_quality(vec3d r[3])
{
	// get the edge lengths
	double l01 = (r[0] - r[1])*(r[0] - r[1]);
	double l12 = (r[1] - r[2])*(r[1] - r[2]);
	double l20 = (r[2] - r[0])*(r[2] - r[0]);

	// get the max edge length
	double lmax = l01;
	if (l12 > lmax) lmax = l12;
	if (l20 > lmax) lmax = l20;

	// calculate the diagonals
	double D[3];
/*	for (int i=0; i<3; ++i)
	{
		int a = i;
		int b = (i+1)%3;
		int c = (i+2)%3;
		vec3d ab = r[b] - r[a];
		vec3d ac = r[c] - r[a];
		double l = (ac*ab)/(ab*ab);
		vec3d p = r[a] + ab*l;
		vec3d pc = r[c] - p;
		D[i] = pc*pc;
	}
*/
	vec3d p, pc;
	vec3d ab = r[1] - r[0];
	vec3d bc = r[2] - r[1];
	vec3d ca = r[0] - r[2];

	double l;
	l = (ca*ab)/(ab*ab);
	pc = ab*l - ca;
	D[0] = pc*pc;

	l = (ab*bc)/(bc*bc);
	pc = bc*l - ab;
	D[1] = pc*pc;

	l = (bc*ca)/(ca*ca);
	pc = ca*l - bc;
	D[2] = pc*pc;

	// get the smallest diagonal
	double dmin = D[0];
	if (D[1] < dmin) dmin = D[1];
	if (D[2] < dmin) dmin = D[2];

	// the quality is the ratio of shortest diagonal to longest edge
	double Q = dmin / lmax;

	return Q;
}

//-----------------------------------------------------------------------------
// Find the quality of the worst triangle
double FEFillHole::min_tri_quality(vector<FACE>& tri)
{
	double qmin = 0.0;
	for (int i=0; i<(int)tri.size(); ++i)
	{
		double q = tri_quality(tri[i].r);
		if (i==0) qmin = q; else qmin = (q<qmin ? q : qmin);
	}
	return qmin;
}

//-----------------------------------------------------------------------------
// find the area of the smallest triangle
double FEFillHole::min_tri_area(vector<FACE>& tri)
{
	double amin = 0.0;
	for (int i=0; i<(int)tri.size(); ++i)
	{
		FACE& fi = tri[i];

		vec3d n = (fi.r[1] - fi.r[0])^(fi.r[2] - fi.r[0]);
		double a = n*n;

		if (i==0) amin = a; else amin = (a < amin ? a : amin);
	}
	return amin;
}

//-----------------------------------------------------------------------------
// Get the right ear of this ring
void FEFillHole::EdgeRing::GetRightEar(int n0, int n1, EdgeRing& ear)
{
	ear.clear();

	// add the first node
	ear.add(m_node[n0], m_r[n0]);

	// add the inside nodes
	int n = n0 + 1;
	if (n >= size()) n = 0;
	do
	{
		ear.add(m_node[n], m_r[n]);
		n++;
		if (n >= size()) n = 0;
	}
	while (n != n1);

	// add the last node
	ear.add(m_node[n1], m_r[n1]);

	// the ear is wound in the same direction
	ear.m_winding = m_winding;
}

//-----------------------------------------------------------------------------
// Get the left ear of this ring
void FEFillHole::EdgeRing::GetLeftEar(int n0, int n1, EdgeRing& ear)
{
	GetRightEar(n1, n0, ear);
}
