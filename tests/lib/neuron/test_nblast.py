import faulthandler
faulthandler.enable()

import os
import numpy as np
from math import isclose
from copy import deepcopy
from time import time

from reneu.lib.libxiuli import XNBLASTScoreTable
from reneu.neuron import Skeleton
from reneu.lib.libxiuli import XVectorCloud, XNBLASTScoreMatrix

# DATA_DIR = os.path.join(os.path.dirname(__file__), '../../../data/')
DATA_DIR = 'data/'
table_path = os.path.join(DATA_DIR, 'smat_fcwb.csv')
st = XNBLASTScoreTable( table_path )
# somehow this is not working
# st = XNBLASTScoreTable()

def test_nblast_score_table():
    assert isclose(st[0., 0.], 9.500096818, abs_tol=1e-4)
    assert isclose(st[50000, 0.93], -10.1287588679926, abs_tol=1e-4)
    assert isclose(st[26000, 0.73], -2.70184701924541, abs_tol=1e-4)
    assert isclose(st[11000, 0.62], 0.28008292141423, abs_tol=1e-4)
    # test the boundary condition
    assert isclose(st[1800, 0.38], 8.23731427922735, abs_tol=1e-4)
    assert isclose(st[15976.5, 1], -0.892506, abs_tol=1e-4)
    assert isclose(st[16011.2, 1], -1.31413, abs_tol=1e-4)
    assert isclose(st[15000, 1], -0.892505829, abs_tol=1e-4)

def test_nblast_with_fake_data():
    point_num = 100
    points = np.zeros((point_num, 3), dtype=np.float32)
    points[:, 2] = np.arange(0, point_num)
    vc = XVectorCloud(points, 10, 2)
    true_vectors = np.repeat(np.array([[0,0,1]]), point_num, axis=0 )
    fake_vectors = deepcopy(vc.vectors)
    # there is a mixture of 1 and -1, both are correct
    fake_vectors[:, 2] = np.abs(fake_vectors[:, 2])
    # print('\nvector cloud: ', vc.vectors)
    # print('\ntrue vector cloud: ', true_vectors)
    np.testing.assert_allclose(fake_vectors, true_vectors, atol=1e-4)

    points2 = deepcopy(points)
    points2[:, 0] += 15000
    vc2 = XVectorCloud(points2, 10, 10)
    fake_vectors = deepcopy(vc.vectors)
    # there is a mixture of 1 and -1, both are correct
    fake_vectors[:, 2] = np.abs(fake_vectors[:, 2])
    np.testing.assert_allclose(fake_vectors, true_vectors, atol=1e-4)

    score = vc.query_by(vc2, st)
    assert isclose(-0.892506 * point_num, score, rel_tol=1e-2)

def test_nblast_with_real_data():   
    print('\n\n start testing nblast with real data.') 
    # the result from R NBLAST is :
    # ID    77625	    77641
    # 77625	86501.20 	53696.72
    # 77641	50891.03 	101011.08
    # Note that R NBLAST use micron as unit, and our NBLAST use nanometer
    # the swc here use nanometer, so no need to divide by 1000
    sk1 = Skeleton.from_swc( os.path.join(DATA_DIR, '77625.swc') )
    sk2 = Skeleton.from_swc( os.path.join(DATA_DIR, '77641.swc') )
    print(f'node number of two neurons: {len(sk1)}, {len(sk2)}')

    print('building vector cloud')
    start = time()
    vc1 = XVectorCloud( sk1.points, 10, 20 )
    print(f'build first vector cloud takes {time()-start} secs.')
    start = time()
    vc2 = XVectorCloud( sk2.points, 10, 20 )
    print(f'build second vector cloud takes {time()-start} secs.')


    print('computing nblast score')
    score12 = vc1.query_by( vc2, st )
    print('nblast score: ', score12, 'with time elapse: ', time()-start, ' sec')
    
    start = time()
    score21 = vc2.query_by( vc1, st )
    print('nblast score: ', score21, 'with time elapse: ', time()-start, ' sec')
    print('as a reference, Julia NBLAST takes about 0.030 sec.')
    assert isclose( score12, 53696.72, rel_tol = 1e-3)
    assert isclose( score21, 50891.03, rel_tol = 1e-3)

    vcs = [ vc1, vc2 ]
    score_matrix = XNBLASTScoreMatrix(vcs, st)
    print('raw scores: ', score_matrix.raw_score_matrix)
    #breakpoint()

if __name__ == '__main__':
    test_nblast_score_table()
    test_nblast_with_fake_data()
    test_nblast_with_real_data()
