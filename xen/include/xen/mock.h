/******************************************************************************
 * mock.h
 *
 * Mock operation interface.
 *
 * Copyright (c) 2019, Bitdefender S.R.L.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __MOCK_H__
#define __MOCK_H__

int mock_domctl(struct domain *d, struct xen_domctl_mock_op *vec);

int mock_get_frames(struct domain *d, unsigned int id,
                    unsigned long frame, unsigned int nr_frames,
                    xen_pfn_t mfn_list[]);

#endif	/* __MOCK_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
