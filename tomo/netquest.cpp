/*
 *	Copyright (C) 2011 Ovidiu Mara
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "netquest.h"

#include <glpk/glpk.h>

void netquestLoss(TomoData &data)
{
	int m = data.m;
	int n = data.n;
	QVector<QVector<quint8> > A = data.A;
	QVector<qreal> y = data.y;
	QVector<qreal> xmeasured = data.xmeasured;

	double lambda = 0.001;

	// hopefully the transmission rate will nto be 0 so we can compute the ln
	for (int i = 0; i < m; i++)
		y[i] = log(y[i]);

	for (int i = 0; i < n; i++)
		xmeasured[i] = log(xmeasured[i]);

	//
	glp_prob *lp;
	lp = glp_create_prob();
	glp_set_prob_name(lp, "tomoloss");

	// we are minimizing
	glp_set_obj_dir(lp, GLP_MIN);

	// we have 2*m row constraints (L1..L2m, U1..U2m)
	glp_add_rows(lp, 2*m);

	// add the 1..m row constraints
	for (int i = 0; i < m; i++) {
		QString rowName = QString("r%1").arg(i);
		glp_set_row_name(lp, 1+i, rowName.toAscii().data());
		// L_i = y[i], U[i] = +Inf
		glp_set_row_bnds(lp, 1+i, GLP_LO, y[i], 0.0);
	}

	// add the m+1..2*m row constraints
	for (int i = m; i < 2*m; i++) {
		QString rowName = QString("r%1").arg(i);
		glp_set_row_name(lp, 1+i, rowName.toAscii().data());
		// L_i = -Inf, U[i] = y[i-m]
		glp_set_row_bnds(lp, 1+i, GLP_UP, 0.0, y[i-m]);
	}

	// we have n+m column constraints, for [x0..xn t0..tm]
	// where |Ax - y| bounded by t
	glp_add_cols(lp, n + m);

	// constraints for x0..xn
	for (int i = 0; i < n; i++) {
		QString colName = QString("x%1").arg(i);
		glp_set_col_name(lp, 1+i, colName.toAscii().data());
		// l_i = -Inf, u[i] = 0
		glp_set_col_bnds(lp, 1+i, GLP_UP, 0.0, 0.0);
	}

	// constraints for t0..xm
	for (int i = n; i < m + n; i++) {
		QString colName = QString("x%1").arg(i);
		glp_set_col_name(lp, 1+i, colName.toAscii().data());
		// l_i = 0, u[i] = +Inf
		glp_set_col_bnds(lp, 1+i, GLP_LO, 0.0, 0.0);
	}

	// enter the coefficients of the objective function
	// c0 = 0, c1..cn = -lambda, cn+1..cn+m = 1
	glp_set_obj_coef(lp, 0, 0.0);

	for (int i = 1; i <= n; i++)
		glp_set_obj_coef(lp, i, -lambda);

	for (int i = n + 1; i <= n + m; i++)
		glp_set_obj_coef(lp, i, 1);

	// enter the coefficients of the row constraints r1..r2*m
	// this is the 2*m x m+n matrix:
	//   A   Im
	//   A  -Im

	for (int i = 0; i < m; i++) {
		QVector<int> indices;
		QVector<double> values;

		// the stupid thing expects arrays starting from 1, add a dummy element at 0
		indices << 0;
		values << 0;

		for (int j = 0; j < n; j++) {
			indices << 1+j;
			values << A[i][j];
		}

		for (int j = n; j < n+m; j++) {
			indices << 1+j;
			values << ((i == j-n) ? 1 : 0);
		}

		glp_set_mat_row(lp, 1+i, n+m, indices.constData(), values.constData());
	}

	for (int i = 0; i < m; i++) {
		QVector<int> indices;
		QVector<double> values;

		// the stupid thing expects arrays starting from 1, add a dummy element at 0
		indices << 0;
		values << 0;

		for (int j = 0; j < n; j++) {
			indices << 1+j;
			values << A[i][j];
		}

		for (int j = n; j < n+m; j++) {
			indices << 1+j;
			values << ((i == j-n) ? -1 : 0);
		}

		glp_set_mat_row(lp, 1+i+m, n+m, indices.constData(), values.constData());
	}

	// solve the problem
	glp_simplex(lp, NULL);

	// request the solution
	double fobj = glp_get_obj_val(lp);
	QList<double> x;
	for (int i = 0; i < n; i++)
		x << glp_get_col_prim(lp, 1+i);

	QList<double> t;
	for (int i = 0; i < m; i++)
		t << glp_get_col_prim(lp, 1+i+n);

	// clean up
	glp_delete_prob(lp);

	qDebug() << "fobj     =" << fobj;
	qDebug() << "t        =" << t;
	qDebug() << "x        =" << x;
	qDebug() << "xmeasured=" << xmeasured;
	for (int i = 0; i < n; i++)
		x[i] = exp(x[i]);
	for (int i = 0; i < n; i++)
		xmeasured[i] = exp(xmeasured[i]);
	qDebug() << "transmission rates:";
	qDebug() << "x        =" << x;
	qDebug() << "xmeasured=" << xmeasured;
}
